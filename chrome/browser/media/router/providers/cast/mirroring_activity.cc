// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"

#include <stdint.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/cast_mirroring_service_host_factory.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/browser/mirroring_to_flinging_switcher.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"
#include "components/media_router/common/route_request_result.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/media_switches.h"
#include "media/cast/cast_config.h"
#include "media/cast/constants.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_address.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"
#include "ui/base/l10n/l10n_util.h"

using blink::mojom::PresentationConnectionMessagePtr;
using cast_channel::Result;
using media_router::mojom::MediaRouteProvider;
using media_router::mojom::MediaRouter;
using mirroring::MirroringServiceHostFactory;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionParameters;
using mirroring::mojom::SessionType;

namespace media_router {

namespace {

constexpr char kHistogramSessionLaunch[] =
    "MediaRouter.CastStreaming.Session.Launch";
constexpr char kHistogramSessionLength[] =
    "MediaRouter.CastStreaming.Session.Length";
constexpr char kHistogramSessionLengthAccessCode[] =
    "MediaRouter.CastStreaming.Session.Length.AccessCode";
constexpr char kHistogramSessionLengthOffscreenTab[] =
    "MediaRouter.CastStreaming.Session.Length.OffscreenTab";
constexpr char kHistogramSessionLengthScreen[] =
    "MediaRouter.CastStreaming.Session.Length.Screen";
constexpr char kHistogramSessionLengthTab[] =
    "MediaRouter.CastStreaming.Session.Length.Tab";
constexpr char kHistogramStartFailureAccessCodeManualEntry[] =
    "MediaRouter.CastStreaming.Start.Failure.AccessCodeManualEntry";
constexpr char kHistogramStartFailureAccessCodeRememberedDevice[] =
    "MediaRouter.CastStreaming.Start.Failure.AccessCodeRememberedDevice";
constexpr char kHistogramStartFailureNative[] =
    "MediaRouter.CastStreaming.Start.Failure.Native";
constexpr char kHistogramStartSuccess[] =
    "MediaRouter.CastStreaming.Start.Success";
constexpr char kHistogramStartSuccessAccessCodeManualEntry[] =
    "MediaRouter.CastStreaming.Start.Success.AccessCodeManualEntry";
constexpr char kHistogramStartSuccessAccessCodeRememberedDevice[] =
    "MediaRouter.CastStreaming.Start.Success.AccessCodeRememberedDevice";

const char kHistogramTypeAudio[] = "Audio";
const char kHistogramTypeVideo[] = "Video";
constexpr char kHistogramTransmissionKbps[] =
    "CastStreaming.Sender.%s.TransmissionRate";
constexpr char kHistogramAverageEncodeTime[] =
    "CastStreaming.Sender.%s.AverageEncodeTime";
constexpr char kHistogramAverageCaptureLatency[] =
    "CastStreaming.Sender.%s.AverageCaptureLatency";
constexpr char kHistogramAverageEndToEndLatency[] =
    "CastStreaming.Sender.%s.AverageEndToEndLatency";
constexpr char kHistogramAverageNetworkLatency[] =
    "CastStreaming.Sender.%s.AverageNetworkLatency";
constexpr char kHistogramRetransmittedPacketsPercentage[] =
    "CastStreaming.Sender.%s.RetransmittedPacketsPercentage";
constexpr char kHistogramExceededPlayoutDelayPacketsPercentage[] =
    "CastStreaming.Sender.%s.ExceededPlayoutDelayPacketsPercentage";
constexpr char kHistogramLateFramesPercentage[] =
    "CastStreaming.Sender.%s.LateFramesPercentage";

constexpr char kLoggerComponent[] = "MirroringService";

using MirroringType = MirroringActivity::MirroringType;

const std::string GetMirroringNamespace(const base::Value::Dict& message) {
  const std::string* type = message.FindString("type");
  if (type &&
      *type == cast_util::EnumToString<cast_channel::CastMessageType,
                                       cast_channel::CastMessageType::kRpc>()) {
    return mirroring::mojom::kRemotingNamespace;
  } else {
    return mirroring::mojom::kWebRtcNamespace;
  }
}

std::optional<MirroringActivity::MirroringType> GetMirroringType(
    const MediaRoute& route) {
  if (!route.is_local()) {
    return std::nullopt;
  }

  const auto source = route.media_source();
  if (source.IsTabMirroringSource()) {
    return MirroringActivity::MirroringType::kTab;
  }
  if (source.IsDesktopMirroringSource()) {
    return MirroringActivity::MirroringType::kDesktop;
  }

  if (base::FeatureList::IsEnabled(media::kMediaRemotingWithoutFullscreen) &&
      source.IsRemotePlaybackSource()) {
    return MirroringActivity::MirroringType::kTab;
  }

  if (!source.url().is_valid()) {
    NOTREACHED_IN_MIGRATION() << "Invalid source: " << source;
    return std::nullopt;
  }

  if (source.IsCastPresentationUrl()) {
    const auto cast_source = CastMediaSource::FromMediaSource(source);
    if (cast_source && cast_source->ContainsStreamingApp()) {
      // Site-initiated Mirroring has a Cast Presentation URL and contains
      // StreamingApp. We should return Tab Mirroring here.
      return MirroringActivity::MirroringType::kTab;
    } else {
      NOTREACHED_IN_MIGRATION() << "Non-mirroring Cast app: " << source;
      return std::nullopt;
    }
  } else if (source.url().SchemeIsHTTPOrHTTPS()) {
    return MirroringActivity::MirroringType::kOffscreenTab;
  }

  NOTREACHED_IN_MIGRATION() << "Invalid source: " << source;
  return std::nullopt;
}

// TODO(crbug.com/1363512): Remove support for sender side letterboxing.
bool ShouldForceLetterboxing(std::string_view model_name) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "disable-cast-letterboxing")) {
    return false;
  }
  return model_name.find("Nest Hub") != std::string_view::npos;
}

std::optional<int> GetExceededPlayoutDelayPacketPercent(
    const base::Value::List* network_latency_ms_histo,
    int64_t target_playout_delay) {
  if (!network_latency_ms_histo) {
    return std::nullopt;
  }

  static constexpr char kOverflowBucketPrefix[] = ">=";
  static constexpr char kBucketDelimiter[] = "-";

  int all_count = 0;
  int exceeded_count = 0;
  for (const base::Value& entry : *network_latency_ms_histo) {
    if (!entry.is_dict() || entry.GetDict().empty()) {
      continue;
    }
    const auto key_value_pair = entry.GetDict().cbegin();
    const std::string& key = key_value_pair->first;
    int count = key_value_pair->second.GetIfDouble().value_or(0);
    if (count == 0) {
      continue;
    }
    all_count += count;

    std::string min_str = "";
    if (key.starts_with(kOverflowBucketPrefix)) {
      min_str = key.substr(2, std::string::npos);
    } else {
      base::StringTokenizer tokenizer(key, kBucketDelimiter);
      if (tokenizer.GetNext()) {
        min_str = tokenizer.token();
      }
    }
    int min = 0;
    if (base::StringToInt(min_str, &min) && min > target_playout_delay) {
      exceeded_count += count;
    }
  }
  if (all_count > 0) {
    return exceeded_count * 100 / all_count;
  }
  return std::nullopt;
}

std::optional<double> LookupStat(
    const base::Value::Dict& mirroring_stats,
    media::cast::StatsEventSubscriber::CastStat cast_stat) {
  const std::string key =
      media::cast::StatsEventSubscriber::CastStatToString(cast_stat);
  return mirroring_stats.FindDouble(key);
}

void MaybeRecordLatencyHistogram(const char* fmt,
                                 const char* streaming_type,
                                 std::optional<double> value) {
  if (value) {
    const std::string name =
        base::StringPrintfNonConstexpr(fmt, streaming_type);
    base::UmaHistogramTimes(name, base::Milliseconds(*value));
  }
}

void MaybeRecordMemoryHistogram(const char* fmt,
                                const char* streaming_type,
                                std::optional<double> value) {
  if (value) {
    const std::string name =
        base::StringPrintfNonConstexpr(fmt, streaming_type);
    base::UmaHistogramMemoryKB(name, *value);
  }
}

void RecordCastStreamingSenderUma(const base::Value::Dict& all_mirroring_stats,
                                  std::string_view stats_dict_key,
                                  int64_t target_playout_delay) {
  const base::Value::Dict* mirroring_stats =
      all_mirroring_stats.FindDict(stats_dict_key);
  if (!mirroring_stats) {
    return;
  }
  const char* streaming_type =
      stats_dict_key == media::cast::StatsEventSubscriber::kAudioStatsDictKey
          ? kHistogramTypeAudio
          : kHistogramTypeVideo;

  const std::optional<double> transmission_kbps = LookupStat(
      *mirroring_stats, media::cast::StatsEventSubscriber::TRANSMISSION_KBPS);
  MaybeRecordMemoryHistogram(kHistogramTransmissionKbps, streaming_type,
                             transmission_kbps);

  const std::optional<double> avg_encode_time_ms = LookupStat(
      *mirroring_stats, media::cast::StatsEventSubscriber::AVG_ENCODE_TIME_MS);
  MaybeRecordLatencyHistogram(kHistogramAverageEncodeTime, streaming_type,
                              avg_encode_time_ms);

  const std::optional<double> avg_capture_latency_ms =
      LookupStat(*mirroring_stats,
                 media::cast::StatsEventSubscriber::AVG_CAPTURE_LATENCY_MS);
  MaybeRecordLatencyHistogram(kHistogramAverageCaptureLatency, streaming_type,
                              avg_capture_latency_ms);

  const std::optional<double> avg_end_to_end_latency_ms = LookupStat(
      *mirroring_stats, media::cast::StatsEventSubscriber::AVG_E2E_LATENCY_MS);
  MaybeRecordLatencyHistogram(kHistogramAverageEndToEndLatency, streaming_type,
                              avg_end_to_end_latency_ms);

  const std::optional<double> avg_network_latency_ms =
      LookupStat(*mirroring_stats,
                 media::cast::StatsEventSubscriber::AVG_NETWORK_LATENCY_MS);
  MaybeRecordLatencyHistogram(kHistogramAverageNetworkLatency, streaming_type,
                              avg_network_latency_ms);

  const std::string num_packets_sent_key =
      media::cast::StatsEventSubscriber::CastStatToString(
          media::cast::StatsEventSubscriber::NUM_PACKETS_SENT);
  const size_t num_packets_sent =
      mirroring_stats->FindDouble(num_packets_sent_key).value_or(0);
  if (num_packets_sent > 0) {
    const std::string num_packets_retransmitted_key =
        media::cast::StatsEventSubscriber::CastStatToString(
            media::cast::StatsEventSubscriber::NUM_PACKETS_RETRANSMITTED);
    const size_t num_packets_retransmitted =
        mirroring_stats->FindDouble(num_packets_retransmitted_key).value_or(0);
    const std::string retransmit_packets_percent_histogram_name =
        base::StringPrintf(kHistogramRetransmittedPacketsPercentage,
                           streaming_type);
    base::UmaHistogramPercentage(
        retransmit_packets_percent_histogram_name,
        num_packets_retransmitted * 100 / num_packets_sent);
  }

  const std::string network_latency_ms_histo_key =
      media::cast::StatsEventSubscriber::CastStatToString(
          media::cast::StatsEventSubscriber::NETWORK_LATENCY_MS_HISTO);
  const base::Value::List* network_latency_ms_histo =
      mirroring_stats->FindList(network_latency_ms_histo_key);
  std::optional<int> exceeded_playout_percent =
      GetExceededPlayoutDelayPacketPercent(network_latency_ms_histo,
                                           target_playout_delay);
  if (exceeded_playout_percent.has_value()) {
    const std::string exceeded_playout_delay_packets_percent_histogram_name =
        base::StringPrintf(kHistogramExceededPlayoutDelayPacketsPercentage,
                           streaming_type);
    base::UmaHistogramPercentage(
        exceeded_playout_delay_packets_percent_histogram_name,
        exceeded_playout_percent.value());
  }

  const double num_frames_captured =
      LookupStat(*mirroring_stats,
                 media::cast::StatsEventSubscriber::NUM_FRAMES_CAPTURED)
          .value_or(0.0);
  if (num_frames_captured) {
    const double num_frames_late =
        LookupStat(*mirroring_stats,
                   media::cast::StatsEventSubscriber::NUM_FRAMES_LATE)
            .value_or(-1.0);
    if (num_frames_late >= 0) {
      const std::string late_frames_percent_histogram_name =
          base::StringPrintf(kHistogramLateFramesPercentage, streaming_type);
      base::UmaHistogramPercentage(late_frames_percent_histogram_name,
                                   num_frames_late * 100 / num_frames_captured);
    }
  }
}

}  // namespace

MirroringActivity::MirroringActivity(
    const MediaRoute& route,
    const std::string& app_id,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker,
    content::FrameTreeNodeId frame_tree_node_id,
    const CastSinkExtraData& cast_data,
    OnStopCallback callback,
    OnSourceChangedCallback source_changed_callback)
    : CastActivity(route, app_id, message_handler, session_tracker),
      media_status_(mojom::MediaStatus::New()),
      mirroring_type_(GetMirroringType(route)),
      frame_tree_node_id_(frame_tree_node_id),
      cast_data_(cast_data),
      on_stop_(std::move(callback)),
      source_changed_callback_(std::move(source_changed_callback)) {
  DETACH_FROM_SEQUENCE(ui_sequence_checker_);
}

MirroringActivity::~MirroringActivity() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, std::move(host_));

  if (!did_start_mirroring_timestamp_) {
    return;
  }

  // Record mirroring pause metrics.
  if (mirroring_pause_timestamp_) {  // The session is ending while paused.
    AccessCodeCastMetrics::RecordMirroringPauseDuration(
        base::Time::Now() - mirroring_pause_timestamp_.value());
  }
  // We can only get a profile on the UI thread, so we must post a task to check
  // if we should log certain metrics.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](int pause_count) {
            // Don't record pause count if the cast session cannot be paused.
            if (pause_count > 0 ||
                media_router::IsAccessCodeCastFreezeUiEnabled(
                    ProfileManager::GetLastUsedProfileIfLoaded())) {
              AccessCodeCastMetrics::RecordMirroringPauseCount(pause_count);
            }
          },
          mirroring_pause_count_));

  auto cast_duration = base::Time::Now() - *did_start_mirroring_timestamp_;
  base::UmaHistogramLongTimes(kHistogramSessionLength, cast_duration);

  const int64_t target_playout_delay_ms =
      target_playout_delay_.has_value()
          ? target_playout_delay_->InMilliseconds()
          : media::cast::kDefaultTargetPlayoutDelay.InMilliseconds();
  RecordCastStreamingSenderUma(
      most_recent_mirroring_stats_,
      media::cast::StatsEventSubscriber::kAudioStatsDictKey,
      target_playout_delay_ms);
  RecordCastStreamingSenderUma(
      most_recent_mirroring_stats_,
      media::cast::StatsEventSubscriber::kVideoStatsDictKey,
      target_playout_delay_ms);

  if (!mirroring_type_) {
    // The mirroring activity should always be set by now, but check anyway
    // to avoid risk of a segfault.
    return;
  }
  switch (*mirroring_type_) {
    case MirroringType::kTab:
      base::UmaHistogramLongTimes(kHistogramSessionLengthTab, cast_duration);
      break;
    case MirroringType::kDesktop:
      base::UmaHistogramLongTimes(kHistogramSessionLengthScreen, cast_duration);
      break;
    case MirroringType::kOffscreenTab:
      base::UmaHistogramLongTimes(kHistogramSessionLengthOffscreenTab,
                                  cast_duration);
      break;
  }
  CastDiscoveryType discovery_type = cast_data_.discovery_type;
  if (discovery_type == CastDiscoveryType::kAccessCodeManualEntry ||
      discovery_type == CastDiscoveryType::kAccessCodeRememberedDevice) {
    base::UmaHistogramLongTimes(kHistogramSessionLengthAccessCode,
                                cast_duration);
  }
}

void MirroringActivity::CreateMojoBindings(mojom::MediaRouter* media_router) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  media_router->GetLogger(logger_.BindNewPipeAndPassReceiver());
  media_router->GetDebugger(debugger_.BindNewPipeAndPassReceiver());

  DCHECK(!channel_to_service_receiver_);
  channel_to_service_receiver_ =
      channel_to_service_.BindNewPipeAndPassReceiver();
}

void MirroringActivity::CreateMirroringServiceHost(
    mirroring::MirroringServiceHostFactory* host_factory_for_test) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (!mirroring_type_) {
    return;
  }

  // base::Unretained use is fine, since it is used with a
  // base::NoDestructor<mirroring::CastMirroringServiceHostFactory> instance.
  auto host_factory = base::Unretained(
      host_factory_for_test
          ? host_factory_for_test
          : &mirroring::CastMirroringServiceHostFactory::GetInstance());

  base::OnceCallback<std::unique_ptr<mirroring::MirroringServiceHost>()>
      host_creation_task;

  // Get a reference to the mirroring service host.
  switch (*mirroring_type_) {
    case MirroringType::kDesktop: {
      auto stream_id = route_.media_source().DesktopStreamId();
      DCHECK(stream_id);
      host_creation_task = base::BindOnce(
          &MirroringServiceHostFactory::GetForDesktop, host_factory, stream_id);
      break;
    }
    case MirroringType::kTab:
      host_creation_task =
          base::BindOnce(&MirroringServiceHostFactory::GetForTab, host_factory,
                         frame_tree_node_id_);
      break;
    case MirroringType::kOffscreenTab:
      host_creation_task =
          base::BindOnce(&MirroringServiceHostFactory::GetForOffscreenTab,
                         host_factory, route_.media_source().url(),
                         route_.presentation_id(), frame_tree_node_id_);
      break;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(host_creation_task)
                     .Then(base::BindOnce(&MirroringActivity::set_host,
                                          weak_ptr_factory_.GetWeakPtr())));
}

void MirroringActivity::OnError(SessionError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  logger_->LogError(
      media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
      base::StringPrintf(
          "Mirroring will stop. MirroringService.SessionError: %d",
          static_cast<int>(error)),
      route_.media_sink_id(), route_.media_source().id(),
      route_.presentation_id());
  if (will_start_mirroring_timestamp_) {
    // An error was encountered while attempting to start mirroring.
    base::UmaHistogramEnumeration(kHistogramStartFailureNative, error);

    // Record the error for access code discovery types.
    CastDiscoveryType discovery_type = cast_data_.discovery_type;
    if (discovery_type == CastDiscoveryType::kAccessCodeManualEntry) {
      base::UmaHistogramEnumeration(kHistogramStartFailureAccessCodeManualEntry,
                                    error);
    } else if (discovery_type ==
               CastDiscoveryType::kAccessCodeRememberedDevice) {
      base::UmaHistogramEnumeration(
          kHistogramStartFailureAccessCodeRememberedDevice, error);
    }

    will_start_mirroring_timestamp_.reset();
  }
  // Metrics for general errors are captured by the mirroring service in
  // MediaRouter.MirroringService.SessionError.
  StopMirroring();
}

void MirroringActivity::DidStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (!will_start_mirroring_timestamp_) {
    // DidStart() was called unexpectedly.
    return;
  }
  did_start_mirroring_timestamp_ = base::Time::Now();
  base::UmaHistogramTimes(
      kHistogramSessionLaunch,
      *did_start_mirroring_timestamp_ - *will_start_mirroring_timestamp_);
  DCHECK(mirroring_type_);
  base::UmaHistogramEnumeration(kHistogramStartSuccess, *mirroring_type_);

  // Record successes to access code discovery types.
  CastDiscoveryType discovery_type = cast_data_.discovery_type;
  if (discovery_type == CastDiscoveryType::kAccessCodeManualEntry) {
    base::UmaHistogramEnumeration(kHistogramStartSuccessAccessCodeManualEntry,
                                  *mirroring_type_);
  } else if (discovery_type == CastDiscoveryType::kAccessCodeRememberedDevice) {
    base::UmaHistogramEnumeration(
        kHistogramStartSuccessAccessCodeRememberedDevice, *mirroring_type_);
  }

  will_start_mirroring_timestamp_.reset();

  if (should_fetch_stats_on_start_) {
    ScheduleFetchMirroringStats();
  }
}

void MirroringActivity::DidStop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  StopMirroring();
}

void MirroringActivity::LogInfoMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  logger_->LogInfo(media_router::mojom::LogCategory::kMirroring,
                   kLoggerComponent, message, route_.media_sink_id(),
                   route_.media_source().id(), route_.presentation_id());
}

void MirroringActivity::LogErrorMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  logger_->LogError(media_router::mojom::LogCategory::kMirroring,
                    kLoggerComponent, message, route_.media_sink_id(),
                    route_.media_source().id(), route_.presentation_id());
}

void MirroringActivity::OnSourceChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (!host_) {
    return;
  }

  std::optional<content::FrameTreeNodeId> frame_tree_node_id =
      host_->GetTabSourceId();
  if (!source_changed_callback_ || !frame_tree_node_id ||
      frame_tree_node_id == frame_tree_node_id_) {
    return;
  }

  source_changed_callback_.Run(frame_tree_node_id_, *frame_tree_node_id);
  frame_tree_node_id_ = *frame_tree_node_id;

  // The source changed, which means that a new capturer was created that is
  // now sending frames. Ensure the state is now PLAYING.
  media_status_->play_state = mojom::MediaStatus::PlayState::PLAYING;
  OnMirroringResumed();
  NotifyMediaStatusObservers();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SwitchToFlingingIfPossible, frame_tree_node_id_));
}

void MirroringActivity::OnRemotingStateChanged(bool is_remoting) {
  media_status_->can_play_pause = !is_remoting;
  // Transitions to/from remoting restart the capturer. Set the state to
  // playing.
  media_status_->play_state = mojom::MediaStatus::PlayState::PLAYING;
  OnMirroringResumed();
  NotifyMediaStatusObservers();
}

void MirroringActivity::OnMessage(mirroring::mojom::CastMessagePtr message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  DCHECK(message);
  DVLOG(2) << "Relaying message to receiver: " << message->json_format_data;

  GetDataDecoder().ParseJson(
      message->json_format_data,
      base::BindOnce(&MirroringActivity::HandleParseJsonResult,
                     weak_ptr_factory_.GetWeakPtr(), route().media_route_id()));
}

void MirroringActivity::OnAppMessage(
    const openscreen::cast::proto::CastMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (!route_.is_local()) {
    return;
  }
  if (message.namespace_() != mirroring::mojom::kWebRtcNamespace &&
      message.namespace_() != mirroring::mojom::kRemotingNamespace) {
    // Ignore message with wrong namespace.
    DVLOG(2) << "Ignoring message with namespace " << message.namespace_();
    return;
  }
  CastSession* session = GetSession();
  if (!session) {
    DVLOG(2) << "No valid session.";
    return;
  }

  if (message.destination_id() != session->destination_id() &&
      message.destination_id() != "*") {
    // Ignore messages sent to someone else.
    DVLOG(2) << "Ignoring message intended for destination_id:\""
             << message.destination_id() << "\" (expected \""
             << session->destination_id() << "\").";
    return;
  }

  if (message.source_id() != message_handler_->source_id()) {
    // Ignore messages sent by a stranger.
    DVLOG(2) << "Ignoring message unexpectedly sent by source_id: \""
             << message.source_id() << "\" (expected \""
             << message_handler_->source_id() << "\")";
    return;
  }

  DVLOG(2) << "Relaying app message from receiver: " << message.DebugString();
  DCHECK(message.has_payload_utf8());
  DCHECK_EQ(message.protocol_version(),
            openscreen::cast::proto::CastMessage_ProtocolVersion_CASTV2_1_0);
  if (message.namespace_() == mirroring::mojom::kWebRtcNamespace) {
    logger_->LogInfo(media_router::mojom::LogCategory::kMirroring,
                     kLoggerComponent,
                     base::StrCat({"Relaying app message from receiver:",
                                   message.payload_utf8()}),
                     route().media_sink_id(), route().media_source().id(),
                     route().presentation_id());
  }

  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.namespace_();
  ptr->json_format_data = message.payload_utf8();
  channel_to_service_->OnMessage(std::move(ptr));
}

void MirroringActivity::OnInternalMessage(
    const cast_channel::InternalMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (!route_.is_local()) {
    return;
  }
  DVLOG(2) << "Relaying internal message from receiver: " << message.message;
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.message_namespace;
  CHECK(base::JSONWriter::Write(message.message, &ptr->json_format_data));
  if (message.message_namespace == mirroring::mojom::kWebRtcNamespace) {
    logger_->LogInfo(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StrCat({"Relaying internal WebRTC message from receiver: ",
                      ptr->json_format_data}),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
  }
  channel_to_service_->OnMessage(std::move(ptr));
}

void MirroringActivity::BindMediaController(
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  media_controller_receivers_.Add(this, std::move(media_controller));
  media_status_observers_.Add(std::move(observer));
}

std::string MirroringActivity::GetRouteDescription(
    const CastSession& session) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (!mirroring_type_) {
    return CastActivity::GetRouteDescription(session);
  }
  switch (*mirroring_type_) {
    case MirroringActivity::MirroringType::kTab:
      return l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_CASTING_TAB);
    case MirroringActivity::MirroringType::kDesktop:
      return l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_CASTING_DESKTOP);
    case MirroringActivity::MirroringType::kOffscreenTab:
      return l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_PRESENTATION_ROUTE_DESCRIPTION,
          base::UTF8ToUTF16(route().media_source().url().host()));
  }
}

void MirroringActivity::HandleParseJsonResult(
    const std::string& route_id,
    data_decoder::DataDecoder::ValueOrError result) {
  CastSession* session = GetSession();
  if (!session) {
    // TODO(crbug.com/1457011): If we're reaching here, determine why.
    logger_->LogError(media_router::mojom::LogCategory::kMirroring,
                      kLoggerComponent,
                      base::StrCat({"Failed to retrieve the session."}),
                      route().media_sink_id(), route().media_source().id(),
                      route().presentation_id());
    return;
  }

  if (!result.has_value() || !result.value().is_dict()) {
    // TODO(crbug.com/41426190): Record UMA metric for parse result.
    logger_->LogError(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StrCat({"Failed to parse Cast client message:", result.error()}),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
    return;
  }

  const std::string message_namespace =
      GetMirroringNamespace(result.value().GetDict());
  if (message_namespace == mirroring::mojom::kWebRtcNamespace) {
    logger_->LogInfo(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StrCat({"WebRTC message received: ",
                      GetScrubbedLogMessage(result.value().GetDict())}),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
  }

  openscreen::cast::proto::CastMessage cast_message =
      cast_channel::CreateCastMessage(message_namespace, std::move(*result),
                                      message_handler_->source_id(),
                                      session->destination_id());
  if (message_handler_->SendCastMessage(cast_data_.cast_channel_id,
                                        cast_message) == Result::kFailed) {
    logger_->LogError(
        media_router::mojom::LogCategory::kMirroring, kLoggerComponent,
        base::StringPrintf(
            "Failed to send Cast message to channel_id: %d, in namespace: %s",
            cast_data_.cast_channel_id, message_namespace.c_str()),
        route().media_sink_id(), route().media_source().id(),
        route().presentation_id());
  }
}

void MirroringActivity::OnSessionSet(const CastSession& session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (!mirroring_type_) {
    return;
  }
  // We use unretained here because weak pointers may be passed safely between
  // sequences, but must always be dereferenced and invalidated on the same
  // SequencedTaskRunner otherwise checking the pointer would be racey. See more
  // at base/memory/weak_ptr.h.
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce(&MirroringActivity::StartSession, base::Unretained(this),
                     session.destination_id()));
}
void MirroringActivity::StartSession(const std::string& destination_id,
                                     bool enable_rtcp_reporting) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  auto cast_source = CastMediaSource::FromMediaSource(route_.media_source());
  DCHECK(cast_source);

  // Derive session type by intersecting the sink capabilities with what the
  // media source can provide.
  const bool has_audio = cast_data_.capabilities.Has(
                             cast_channel::CastDeviceCapability::kAudioOut) &&
                         cast_source->ProvidesStreamingAudioCapture();
  const bool has_video = cast_data_.capabilities.Has(
      cast_channel::CastDeviceCapability::kVideoOut);
  if (!has_audio && !has_video) {
    return;
  }
  const SessionType session_type = has_audio && has_video
                                       ? SessionType::AUDIO_AND_VIDEO
                                   : has_audio ? SessionType::AUDIO_ONLY
                                               : SessionType::VIDEO_ONLY;

  will_start_mirroring_timestamp_ = base::Time::Now();

  // Bind Mojo receivers for the interfaces this object implements.
  mojo::PendingRemote<mirroring::mojom::SessionObserver> observer_remote;
  observer_receiver_.Bind(observer_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mirroring::mojom::CastMessageChannel> channel_remote;
  channel_receiver_.Bind(channel_remote.InitWithNewPipeAndPassReceiver());

  // If the target playout delay has not yet been set (from site-initiated
  // mirroring request) then try to set it from a feature or commandline.
  target_playout_delay_ = cast_source->target_playout_delay().has_value()
                              ? cast_source->target_playout_delay()
                              : GetCastMirroringPlayoutDelay();
  should_fetch_stats_on_start_ = enable_rtcp_reporting;

  // If this fails, it's probably because CreateMojoBindings() hasn't been
  // called.
  DCHECK(channel_to_service_receiver_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MirroringActivity::StartOnUiThread, weak_ptr_factory_.GetWeakPtr(),
          SessionParameters::New(
              session_type, cast_data_.ip_endpoint.address(),
              cast_data_.model_name, sink_.sink().name(), destination_id,
              message_handler_->source_id(), target_playout_delay_,
              route().media_source().IsRemotePlaybackSource(),
              ShouldForceLetterboxing(cast_data_.model_name),
              enable_rtcp_reporting),
          std::move(observer_remote), std::move(channel_remote),
          std::move(channel_to_service_receiver_), route_.media_sink_name()));
}

void MirroringActivity::StartOnUiThread(
    mirroring::mojom::SessionParametersPtr session_params,
    mojo::PendingRemote<mirroring::mojom::SessionObserver> observer,
    mojo::PendingRemote<mirroring::mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mirroring::mojom::CastMessageChannel> inbound_channel,
    const std::string& sink_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!host_) {
    return;
  }

  host_->Start(std::move(session_params), std::move(observer),
               std::move(outbound_channel), std::move(inbound_channel),
               sink_name);
}

void MirroringActivity::StopMirroring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  // Running the callback will cause this object to be deleted.
  if (on_stop_) {
    std::move(on_stop_).Run();
  }
}

std::string MirroringActivity::GetScrubbedLogMessage(
    const base::Value::Dict& message) {
  std::string message_str;
  auto scrubbed_message = message.Clone();
  base::Value::List* streams =
      scrubbed_message.FindListByDottedPath("offer.supportedStreams");
  if (!streams) {
    base::JSONWriter::Write(scrubbed_message, &message_str);
    return message_str;
  }

  for (base::Value& item : *streams) {
    if (!item.is_dict()) {
      continue;
    }
    if (item.GetDict().FindString("aesKey")) {
      item.GetDict().Set("aesKey", "AES_KEY");
    }
    if (item.GetDict().FindString("aesIvMask")) {
      item.GetDict().Set("aesIvMask", "AES_IV_MASK");
    }
  }
  base::JSONWriter::Write(scrubbed_message, &message_str);
  return message_str;
}

void MirroringActivity::ScheduleFetchMirroringStats() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  // When a mirroring route starts, create a mirroring stats fetch loop every
  // kRtcpReportInterval, which is the same interval that the logger will send
  // stats data.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MirroringActivity::FetchMirroringStats,
                     weak_ptr_factory_.GetWeakPtr()),
      media::cast::kRtcpReportInterval);
}

void MirroringActivity::FetchMirroringStats() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  // Only fetch mirroring stats if our feature is still enabled AND if the
  // current mirroring route still exits.
  if (!should_fetch_stats_on_start_ || !host_) {
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&mirroring::MirroringServiceHost::GetMirroringStats,
                     host_->GetWeakPtr(),
                     base::BindPostTaskToCurrentDefault(
                         base::BindOnce(&MirroringActivity::OnMirroringStats,
                                        weak_ptr_factory_.GetWeakPtr()))));

  ScheduleFetchMirroringStats();
}

void MirroringActivity::OnMirroringStats(base::Value json_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  debugger_->OnMirroringStats(json_stats.Clone());
  if (json_stats.is_dict()) {
    most_recent_mirroring_stats_ = std::move(json_stats.GetDict());
  }
}

void MirroringActivity::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (host_) {
    base::OnceCallback<void()> cb = base::BindOnce(
        &MirroringActivity::SetPlayState, weak_ptr_factory_.GetWeakPtr(),
        mojom::MediaStatus::PlayState::PLAYING);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&mirroring::MirroringServiceHost::Resume,
                                  host_->GetWeakPtr(), std::move(cb)));
  }
}

void MirroringActivity::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  if (host_) {
    base::OnceCallback<void()> cb = base::BindOnce(
        &MirroringActivity::SetPlayState, weak_ptr_factory_.GetWeakPtr(),
        mojom::MediaStatus::PlayState::PAUSED);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&mirroring::MirroringServiceHost::Pause,
                                  host_->GetWeakPtr(), std::move(cb)));
  }
}

void MirroringActivity::SetPlayState(mojom::MediaStatus::PlayState play_state) {
  media_status_->play_state = play_state;
  if (play_state == mojom::MediaStatus::PlayState::PLAYING) {
    OnMirroringResumed();
  } else if (play_state == mojom::MediaStatus::PlayState::PAUSED) {
    OnMirroringPaused();
  }
  NotifyMediaStatusObservers();
}

void MirroringActivity::NotifyMediaStatusObservers() {
  for (const mojo::Remote<mojom::MediaStatusObserver>& observer :
       media_status_observers_) {
    observer->OnMediaStatusUpdated(media_status_.Clone());
  }
}

void MirroringActivity::OnMirroringPaused() {
  // Do nothing if we are already paused.
  if (mirroring_pause_timestamp_) {
    return;
  }
  mirroring_pause_timestamp_ = base::Time::Now();
  mirroring_pause_count_++;
}

void MirroringActivity::OnMirroringResumed() {
  if (!mirroring_pause_timestamp_) {
    return;
  }
  AccessCodeCastMetrics::RecordMirroringPauseDuration(
      base::Time::Now() - mirroring_pause_timestamp_.value());
  mirroring_pause_timestamp_.reset();
}

}  // namespace media_router
