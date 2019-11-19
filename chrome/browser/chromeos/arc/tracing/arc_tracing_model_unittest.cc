// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event_matcher.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace arc {

namespace {

using GraphicsEvents = ArcTracingGraphicsModel::BufferEvents;
using GraphicsEventType = ArcTracingGraphicsModel::BufferEventType;

constexpr char kAcquireBufferQuery[] =
    "android:onMessageReceived/android:handleMessageInvalidate/"
    "android:latchBuffer/android:updateTexImage/android:acquireBuffer";

constexpr char kAttachSurfaceQueury[] =
    "toplevel:OnLibevent/exo:Surface::Attach";

constexpr char kTestEvent[] =
    R"({"pid":4640,"tid":4641,"id":"1234","ts":14241877057,"ph":"X","cat":"exo",
        "name":"Surface::Attach",
        "args":{"buffer_id":"0x7f9f5110690","app_id":"org.chromium.arc.6"},
        "dur":10,"tdur":9,"tts":1216670360})";

constexpr char kAppId[] = "app_id";
constexpr char kAppIdValue[] = "org.chromium.arc.6";
constexpr char kBufferId[] = "buffer_id";
constexpr char kBufferIdBad[] = "_buffer_id";
constexpr char kBufferIdValue[] = "0x7f9f5110690";
constexpr char kBufferIdValueBad[] = "_0x7f9f5110690";
constexpr char kDefault[] = "default";
constexpr char kExo[] = "exo";
constexpr char kExoBad[] = "_exo";
constexpr char kPhaseX = 'X';
constexpr char kPhaseP = 'P';
constexpr char kSurfaceAttach[] = "Surface::Attach";
constexpr char kSurfaceAttachBad[] = "_Surface::Attach";

// Validates that events have increasing timestamp, and all events have allowed
// transitions from the previous state.
bool ValidateCpuEvents(const CpuEvents& cpu_events) {
  if (cpu_events.empty())
    return false;

  CpuEvents cpu_events_reconstructed;
  for (const auto& cpu_event : cpu_events) {
    if (!AddCpuEvent(&cpu_events_reconstructed, cpu_event.timestamp,
                     cpu_event.type, cpu_event.tid)) {
      return false;
    }
  }

  return cpu_events_reconstructed == cpu_events;
}

// Validates that events have increasing timestamp, have only allowed types and
// each type is found at least once.
bool ValidateGrahpicsEvents(const GraphicsEvents& events,
                            const std::set<GraphicsEventType>& allowed_types) {
  if (events.empty())
    return false;
  uint64_t previous_timestamp = 0;
  std::set<GraphicsEventType> used_types;
  for (const auto& event : events) {
    if (event.timestamp < previous_timestamp) {
      LOG(ERROR) << "Timestamp sequence broken: " << event.timestamp << " vs "
                 << previous_timestamp << ".";
      return false;
    }
    previous_timestamp = event.timestamp;
    if (!allowed_types.count(event.type)) {
      LOG(ERROR) << "Unexpected event type " << event.type << ".";
      return false;
    }
    used_types.insert(event.type);
  }
  if (used_types.size() != allowed_types.size()) {
    for (const auto& allowed_type : allowed_types) {
      if (!used_types.count(allowed_type))
        LOG(ERROR) << "Required event type " << allowed_type
                   << " << is not found.";
    }
    return false;
  }
  return true;
}

std::unique_ptr<ArcTracingGraphicsModel> LoadGraphicsModel(
    const std::string& name) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath tracing_path =
      base_path.Append("arc_graphics_tracing").Append(name);
  std::string json_data;
  base::ReadFileToString(tracing_path, &json_data);
  DCHECK(!json_data.empty());
  std::unique_ptr<ArcTracingGraphicsModel> model =
      std::make_unique<ArcTracingGraphicsModel>();
  if (!model->LoadFromJson(json_data))
    return nullptr;
  return model;
}

// Ensures |model1| is equal to |model2|.
void EnsureGraphicsModelsEqual(const ArcTracingGraphicsModel& model1,
                               const ArcTracingGraphicsModel& model2) {
  EXPECT_EQ(model1.android_top_level(), model2.android_top_level());
  EXPECT_EQ(model1.chrome_top_level(), model2.chrome_top_level());
  EXPECT_EQ(model1.view_buffers(), model2.view_buffers());
  EXPECT_EQ(model1.system_model(), model2.system_model());
  EXPECT_EQ(model1.duration(), model2.duration());
  EXPECT_EQ(model1.app_title(), model2.app_title());
  EXPECT_EQ(model1.app_icon_png(), model2.app_icon_png());
  EXPECT_EQ(model1.timestamp(), model2.timestamp());
  EXPECT_EQ(model1.platform(), model2.platform());
}

}  // namespace

class ArcTracingModelTest : public testing::Test {
 public:
  ArcTracingModelTest() = default;
  ~ArcTracingModelTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcTracingModelTest);
};

// TopLevel test is performed on the data collected from the real test device.
// It tests that model can be built from the provided data and queries work.
TEST_F(ArcTracingModelTest, TopLevel) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath tracing_path =
      base_path.Append("arc_graphics_tracing").Append("trace.dat.gz");

  std::string tracing_data_compressed;
  ASSERT_TRUE(base::ReadFileToString(tracing_path, &tracing_data_compressed));

  std::string tracing_data;
  ASSERT_TRUE(
      compression::GzipUncompress(tracing_data_compressed, &tracing_data));

  ArcTracingModel model;
  ASSERT_TRUE(model.Build(tracing_data));

  // 4 CPU cores.
  EXPECT_EQ(4U, model.system_model().all_cpu_events().size());
  for (const auto& cpu_events : model.system_model().all_cpu_events())
    EXPECT_TRUE(ValidateCpuEvents(cpu_events));

  // Perform several well-known queries.
  EXPECT_FALSE(model.Select(kAcquireBufferQuery).empty());
  EXPECT_FALSE(model.Select(kAttachSurfaceQueury).empty());

  std::stringstream ss;
  model.Dump(ss);
  EXPECT_FALSE(ss.str().empty());

  // Continue in this test to avoid heavy calculations for building base model.
  // Make sure we can create graphics model.
  ArcTracingGraphicsModel graphics_model;
  ASSERT_TRUE(graphics_model.Build(model));

  ASSERT_EQ(1U, graphics_model.android_top_level().buffer_events().size());
  EXPECT_TRUE(ValidateGrahpicsEvents(
      graphics_model.android_top_level().buffer_events()[0],
      {GraphicsEventType::kSurfaceFlingerInvalidationStart,
       GraphicsEventType::kSurfaceFlingerInvalidationDone,
       GraphicsEventType::kSurfaceFlingerCompositionStart,
       GraphicsEventType::kSurfaceFlingerCompositionDone}));
  EXPECT_TRUE(
      ValidateGrahpicsEvents(graphics_model.android_top_level().global_events(),
                             {GraphicsEventType::kSurfaceFlingerVsyncHandler,
                              GraphicsEventType::kSurfaceFlingerCompositionJank,
                              GraphicsEventType::kVsyncTimestamp}));
  ASSERT_FALSE(graphics_model.android_top_level().global_events().empty());
  // Check trimmed by VSYNC.
  EXPECT_EQ(GraphicsEventType::kSurfaceFlingerVsyncHandler,
            graphics_model.android_top_level().global_events()[0].type);
  EXPECT_EQ(0U,
            graphics_model.android_top_level().global_events()[0].timestamp);

  EXPECT_EQ(2U, graphics_model.chrome_top_level().buffer_events().size());
  for (const auto& chrome_top_level_band :
       graphics_model.chrome_top_level().buffer_events()) {
    EXPECT_TRUE(ValidateGrahpicsEvents(
        chrome_top_level_band, {
                                   GraphicsEventType::kChromeOSDraw,
                                   GraphicsEventType::kChromeOSSwap,
                                   GraphicsEventType::kChromeOSWaitForAck,
                                   GraphicsEventType::kChromeOSPresentationDone,
                                   GraphicsEventType::kChromeOSSwapDone,
                               }));
  }
  EXPECT_FALSE(graphics_model.view_buffers().empty());
  for (const auto& view : graphics_model.view_buffers()) {
    // At least one buffer.
    EXPECT_GT(view.first.task_id, 0);
    EXPECT_NE(std::string(), view.first.activity);
    EXPECT_FALSE(view.second.buffer_events().empty());
    for (const auto& buffer : view.second.buffer_events()) {
      EXPECT_TRUE(ValidateGrahpicsEvents(
          buffer, {
                      GraphicsEventType::kBufferQueueDequeueStart,
                      GraphicsEventType::kBufferQueueDequeueDone,
                      GraphicsEventType::kBufferQueueQueueStart,
                      GraphicsEventType::kBufferQueueQueueDone,
                      GraphicsEventType::kBufferQueueAcquire,
                      GraphicsEventType::kBufferQueueReleased,
                      GraphicsEventType::kExoSurfaceAttach,
                      GraphicsEventType::kExoProduceResource,
                      GraphicsEventType::kExoBound,
                      GraphicsEventType::kExoPendingQuery,
                      GraphicsEventType::kExoReleased,
                      GraphicsEventType::kChromeBarrierOrder,
                      GraphicsEventType::kChromeBarrierFlush,
                  }));
    }
  }

  // Note, CPU events in |graphics_model| are normalized by timestamp. So they
  // are not equal and we cannot do direct comparison. Also first VSYNC event
  // trimming may drop some events.
  ASSERT_LE(graphics_model.system_model().all_cpu_events().size(),
            model.system_model().all_cpu_events().size());
  EXPECT_EQ(graphics_model.system_model().thread_map(),
            model.system_model().thread_map());
  for (size_t i = 0; i < graphics_model.system_model().all_cpu_events().size();
       ++i) {
    EXPECT_LE(graphics_model.system_model().all_cpu_events()[i].size(),
              model.system_model().all_cpu_events()[i].size());
  }

  EXPECT_GT(graphics_model.duration(), 0U);

  // Serialize and restore;
  const std::string graphics_model_data = graphics_model.SerializeToJson();
  EXPECT_FALSE(graphics_model_data.empty());

  ArcTracingGraphicsModel graphics_model_loaded;
  EXPECT_TRUE(graphics_model_loaded.LoadFromJson(graphics_model_data));

  // Models should match.
  EnsureGraphicsModelsEqual(graphics_model, graphics_model_loaded);
}

// Validates basic system event timestamp processing
TEST_F(ArcTracingModelTest, SystemTraceEventTimestampParsing) {
  {
    std::string tracing_data =
        "{\"traceEvents\":[],\"systemTraceEvents\":\""
        // clang-format off
        "  surfaceflinger-9772  [000] ...0 80156.539255: tracing_mark_write: B|15|acquireBuffer\n"
        // clang-format on
        "\"}";

    ArcTracingModel model;
    ASSERT_TRUE(model.Build(tracing_data));

    const ArcTracingModel::TracingEventPtrs events =
        model.Select("android:acquireBuffer");
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(15, events[0]->GetPid());
    EXPECT_EQ("android", events[0]->GetCategory());
    EXPECT_EQ("acquireBuffer", events[0]->GetName());
    EXPECT_EQ('X', events[0]->GetPhase());
    EXPECT_EQ(80156539255UL, events[0]->GetTimestamp());
    EXPECT_EQ(0U, events[0]->GetDuration());
  }

  {
    // Too few digits after the timestamp decimal point should be an error
    std::string bad_tracing_data_1 =
        "{\"traceEvents\":[],\"systemTraceEvents\":\""
        // clang-format off
        "  surfaceflinger-9772  [000] ...0 999.12345: tracing_mark_write: B|15|acquireBuffer\n"
        // clang-format on
        "\"}";
    ArcTracingModel model;
    EXPECT_FALSE(model.Build(bad_tracing_data_1));
  }

  {
    // Too many digits after the timestamp decimal point should be an error
    std::string bad_tracing_data_2 =
        "{\"traceEvents\":[],\"systemTraceEvents\":\""
        // clang-format off
        "  surfaceflinger-9772  [000] ...0 999.1234567: tracing_mark_write: B|15|acquireBuffer\n"
        // clang-format on
        "\"}";
    ArcTracingModel model;
    EXPECT_FALSE(model.Build(bad_tracing_data_2));
  }
}

TEST_F(ArcTracingModelTest, SystemTraceEventCpuEventProcessing) {
  std::string tracing_data =
      "{\"traceEvents\":[],\"systemTraceEvents\":\""
      // clang-format off
      "          <idle>-0     [000] d..0 123.000001: cpu_idle: state=0 cpu_id=0\n"
      "          <idle>-0     [000] dn.0 123.000002: cpu_idle: state=4294967295 cpu_id=0\n"
      "          <idle>-0     [000] dnh3 123.000003: sched_wakeup: comm=foo pid=15821 prio=115 target_cpu=000\n"
      "          <idle>-0     [000] d..3 123.000004: sched_switch: prev_comm=bar prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=baz next_pid=15821 next_prio=115\n"
      // clang-format on
      "\"}";

  ArcTracingModel model;
  ASSERT_TRUE(model.Build(tracing_data));

  const AllCpuEvents& cpu_events = model.system_model().all_cpu_events();
  ASSERT_EQ(1u, cpu_events.size());
  CpuEvents cpu0_events = cpu_events[0];
  ASSERT_EQ(4u, cpu0_events.size());

  EXPECT_EQ(123000001UL, cpu0_events[0].timestamp);
  EXPECT_EQ(ArcCpuEvent::Type::kIdleIn, cpu0_events[0].type);
  EXPECT_EQ(0u, cpu0_events[0].tid);

  EXPECT_EQ(123000002UL, cpu0_events[1].timestamp);
  EXPECT_EQ(ArcCpuEvent::Type::kIdleOut, cpu0_events[1].type);
  EXPECT_EQ(0u, cpu0_events[1].tid);

  EXPECT_EQ(123000003UL, cpu0_events[2].timestamp);
  EXPECT_EQ(ArcCpuEvent::Type::kWakeUp, cpu0_events[2].type);
  EXPECT_EQ(15821u, cpu0_events[2].tid);

  EXPECT_EQ(123000004UL, cpu0_events[3].timestamp);
  EXPECT_EQ(ArcCpuEvent::Type::kActive, cpu0_events[3].type);
  EXPECT_EQ(15821u, cpu0_events[3].tid);
}

TEST_F(ArcTracingModelTest, Event) {
  const ArcTracingEvent event(base::JSONReader::Read(kTestEvent).value());

  EXPECT_EQ(4640, event.GetPid());
  EXPECT_EQ(4641, event.GetTid());
  EXPECT_EQ("1234", event.GetId());
  EXPECT_EQ(kExo, event.GetCategory());
  EXPECT_EQ(kSurfaceAttach, event.GetName());
  EXPECT_EQ(kPhaseX, event.GetPhase());
  EXPECT_EQ(14241877057UL, event.GetTimestamp());
  EXPECT_EQ(10U, event.GetDuration());
  EXPECT_EQ(14241877067UL, event.GetEndTimestamp());
  EXPECT_NE(nullptr, event.GetDictionary());
  EXPECT_EQ(kBufferIdValue, event.GetArgAsString(kBufferId, std::string()));
  EXPECT_EQ(kDefault, event.GetArgAsString(kBufferIdBad, kDefault));
}

TEST_F(ArcTracingModelTest, EventClassification) {
  const ArcTracingEvent event(base::JSONReader::Read(kTestEvent).value());

  ArcTracingEvent event_before(base::JSONReader::Read(kTestEvent).value());
  event_before.SetTimestamp(event.GetTimestamp() - event.GetDuration());
  EXPECT_EQ(ArcTracingEvent::Position::kBefore,
            event.ClassifyPositionOf(event_before));

  ArcTracingEvent event_after(base::JSONReader::Read(kTestEvent).value());
  event_after.SetTimestamp(event.GetTimestamp() + event.GetDuration());
  EXPECT_EQ(ArcTracingEvent::Position::kAfter,
            event.ClassifyPositionOf(event_after));

  ArcTracingEvent event_inside(base::JSONReader::Read(kTestEvent).value());
  event_inside.SetTimestamp(event.GetTimestamp() + 1);
  event_inside.SetDuration(event.GetDuration() - 2);
  EXPECT_EQ(ArcTracingEvent::Position::kInside,
            event.ClassifyPositionOf(event_inside));
  EXPECT_EQ(ArcTracingEvent::Position::kInside,
            event.ClassifyPositionOf(event));

  ArcTracingEvent event_overlap(base::JSONReader::Read(kTestEvent).value());
  event_overlap.SetTimestamp(event.GetTimestamp() + 1);
  EXPECT_EQ(ArcTracingEvent::Position::kOverlap,
            event.ClassifyPositionOf(event_overlap));
  event_overlap.SetTimestamp(event.GetTimestamp());
  event_overlap.SetDuration(event.GetDuration() + 1);
  EXPECT_EQ(ArcTracingEvent::Position::kOverlap,
            event.ClassifyPositionOf(event_overlap));
  event_overlap.SetTimestamp(event.GetTimestamp() - 1);
  event_overlap.SetDuration(event.GetDuration() + 2);
  EXPECT_EQ(ArcTracingEvent::Position::kOverlap,
            event.ClassifyPositionOf(event_overlap));
}

TEST_F(ArcTracingModelTest, EventAppendChild) {
  ArcTracingEvent event(base::JSONReader::Read(kTestEvent).value());

  // Impossible to append the even that is bigger than target.
  std::unique_ptr<ArcTracingEvent> event_overlap =
      std::make_unique<ArcTracingEvent>(
          base::JSONReader::Read(kTestEvent).value());
  event_overlap->SetTimestamp(event.GetTimestamp() + 1);
  EXPECT_FALSE(event.AppendChild(std::move(event_overlap)));

  std::unique_ptr<ArcTracingEvent> event1 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::Read(kTestEvent).value());
  event1->SetTimestamp(event.GetTimestamp() + 4);
  event1->SetDuration(2);
  EXPECT_TRUE(event.AppendChild(std::move(event1)));
  EXPECT_EQ(1U, event.children().size());

  // Impossible to append the event that is before last child.
  std::unique_ptr<ArcTracingEvent> event2 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::Read(kTestEvent).value());
  event2->SetTimestamp(event.GetTimestamp());
  event2->SetDuration(2);
  EXPECT_FALSE(event.AppendChild(std::move(event2)));
  EXPECT_EQ(1U, event.children().size());

  // Append child to child
  std::unique_ptr<ArcTracingEvent> event3 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::Read(kTestEvent).value());
  event3->SetTimestamp(event.GetTimestamp() + 5);
  event3->SetDuration(1);
  EXPECT_TRUE(event.AppendChild(std::move(event3)));
  ASSERT_EQ(1U, event.children().size());
  EXPECT_EQ(1U, event.children()[0].get()->children().size());

  // Append next immediate child.
  std::unique_ptr<ArcTracingEvent> event4 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::Read(kTestEvent).value());
  event4->SetTimestamp(event.GetTimestamp() + 6);
  event4->SetDuration(2);
  EXPECT_TRUE(event.AppendChild(std::move(event4)));
  EXPECT_EQ(2U, event.children().size());
}

TEST_F(ArcTracingModelTest, EventMatcher) {
  const ArcTracingEvent event(base::JSONReader::Read(kTestEvent).value());
  // Nothing is specified. It matches any event.
  EXPECT_TRUE(ArcTracingEventMatcher().Match(event));

  // Phase
  EXPECT_TRUE(ArcTracingEventMatcher().SetPhase(kPhaseX).Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher().SetPhase(kPhaseP).Match(event));

  // Category
  EXPECT_TRUE(ArcTracingEventMatcher().SetCategory(kExo).Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher().SetCategory(kExoBad).Match(event));

  // Name
  EXPECT_TRUE(ArcTracingEventMatcher().SetName(kSurfaceAttach).Match(event));
  EXPECT_FALSE(
      ArcTracingEventMatcher().SetName(kSurfaceAttachBad).Match(event));

  // Arguments
  EXPECT_TRUE(ArcTracingEventMatcher()
                  .AddArgument(kBufferId, kBufferIdValue)
                  .Match(event));
  EXPECT_TRUE(ArcTracingEventMatcher()
                  .AddArgument(kBufferId, kBufferIdValue)
                  .AddArgument(kAppId, kAppIdValue)
                  .Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher()
                   .AddArgument(kBufferIdBad, kBufferIdValue)
                   .Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher()
                   .AddArgument(kBufferId, kBufferIdValueBad)
                   .Match(event));

  // String query
  EXPECT_TRUE(
      ArcTracingEventMatcher("exo:Surface::Attach(buffer_id=0x7f9f5110690)")
          .Match(event));
  EXPECT_FALSE(
      ArcTracingEventMatcher("_exo:_Surface::Attach(buffer_id=_0x7f9f5110690)")
          .Match(event));
}

TEST_F(ArcTracingModelTest, TimeMinMax) {
  // Model contains events with timestamps 100000..100004 inclusively.
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath tracing_path =
      base_path.Append("arc_graphics_tracing").Append("trace_time.dat");

  std::string tracing_data;
  ASSERT_TRUE(base::ReadFileToString(tracing_path, &tracing_data));

  ArcTracingModel model_without_time_filter;
  EXPECT_TRUE(model_without_time_filter.Build(tracing_data));
  EXPECT_EQ(5U, model_without_time_filter.GetRoots().size());

  ArcTracingModel model_with_time_filter;
  model_with_time_filter.SetMinMaxTime(100001L, 100003L);
  EXPECT_TRUE(model_with_time_filter.Build(tracing_data));
  ASSERT_EQ(2U, model_with_time_filter.GetRoots().size());
  EXPECT_EQ(100001UL, model_with_time_filter.GetRoots()[0]->GetTimestamp());
  EXPECT_EQ(100002UL, model_with_time_filter.GetRoots()[1]->GetTimestamp());

  ArcTracingModel model_with_empty_time_filter;
  model_with_empty_time_filter.SetMinMaxTime(99999L, 100000L);
  EXPECT_TRUE(model_with_empty_time_filter.Build(tracing_data));
  EXPECT_EQ(0U, model_with_empty_time_filter.GetRoots().size());
}

TEST_F(ArcTracingModelTest, GraphicsModelLoadSerialize) {
  std::unique_ptr<ArcTracingGraphicsModel> model =
      LoadGraphicsModel("gm_good.json");
  ASSERT_TRUE(model);
  EXPECT_EQ("CrOS 12642.0.0 (Official Build) dev-channel eve",
            model->platform());
  EXPECT_EQ("Play Store", model->app_title());
  EXPECT_FALSE(model->app_icon_png().empty());
  EXPECT_EQ(base::Time::FromJsTime(1572898642036L), model->timestamp());
  EXPECT_EQ(1000U, model->duration());

  ArcTracingGraphicsModel test_model;
  EXPECT_TRUE(test_model.LoadFromJson(model->SerializeToJson()));
  EnsureGraphicsModelsEqual(*model, test_model);

  EXPECT_TRUE(LoadGraphicsModel("gm_good.json"));
  EXPECT_FALSE(LoadGraphicsModel("gm_bad_no_view_buffers.json"));
  EXPECT_FALSE(LoadGraphicsModel("gm_bad_no_view_desc.json"));
  EXPECT_FALSE(LoadGraphicsModel("gm_bad_wrong_timestamp.json"));
}

TEST_F(ArcTracingModelTest, EventsContainerTrim) {
  ArcTracingGraphicsModel::EventsContainer events;
  constexpr int64_t trim_timestamp = 25;
  events.global_events().emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSJank,
      15 /* timestamp */);
  events.global_events().emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSJank,
      25 /* timestamp */);
  events.global_events().emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSJank,
      30 /* timestamp */);
  events.global_events().emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSJank,
      35 /* timestamp */);
  events.buffer_events().resize(1);
  // Two sequences, first sequence starts before trim and ends after. After
  // trimming  next sequence should be preserved only.
  events.buffer_events()[0].emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSDraw,
      20 /* timestamp */);
  events.buffer_events()[0].emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSSwapDone,
      30 /* timestamp */);
  events.buffer_events()[0].emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSDraw,
      40 /* timestamp */);
  events.buffer_events()[0].emplace_back(
      ArcTracingGraphicsModel::BufferEventType::kChromeOSSwapDone,
      50 /* timestamp */);
  ArcTracingGraphicsModel::TrimEventsContainer(
      &events, trim_timestamp,
      {ArcTracingGraphicsModel::BufferEventType::kChromeOSDraw});
  ASSERT_EQ(3U, events.global_events().size());
  EXPECT_EQ(25U, events.global_events()[0].timestamp);
  ASSERT_EQ(1U, events.buffer_events().size());
  ASSERT_EQ(2U, events.buffer_events()[0].size());
  EXPECT_EQ(40U, events.buffer_events()[0][0].timestamp);
  EXPECT_EQ(ArcTracingGraphicsModel::BufferEventType::kChromeOSDraw,
            events.buffer_events()[0][0].type);
}

TEST_F(ArcTracingModelTest, AsynchronousSystemEvents) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath tracing_path = base_path.Append("arc_graphics_tracing")
                                          .Append("trace_async_events.json");
  std::string tracing_data;
  base::ReadFileToString(tracing_path, &tracing_data);
  DCHECK(!tracing_data.empty());

  ArcTracingModel model;
  ASSERT_TRUE(model.Build(tracing_data));

  const ArcTracingModel::TracingEventPtrs group1 = model.GetGroupEvents("1");
  const ArcTracingModel::TracingEventPtrs group2 = model.GetGroupEvents("2");

  constexpr char kAsync1[] = "async1";
  constexpr char kAsync2[] = "async2";

  ASSERT_EQ(2U, group1.size());
  EXPECT_EQ(kAsync1, group1[0]->GetName());
  EXPECT_EQ(kAsync1, group1[1]->GetName());
  EXPECT_EQ("1", group1[0]->GetId());
  EXPECT_EQ(group1[0]->GetId(), group1[1]->GetId());
  EXPECT_EQ(TRACE_EVENT_PHASE_ASYNC_BEGIN, group1[0]->GetPhase());
  EXPECT_EQ(TRACE_EVENT_PHASE_ASYNC_END, group1[1]->GetPhase());
  EXPECT_EQ(1100000UL, group1[0]->GetTimestamp());
  EXPECT_EQ(1300000UL, group1[1]->GetTimestamp());

  ASSERT_EQ(2U, group2.size());
  EXPECT_EQ(kAsync2, group2[0]->GetName());
  EXPECT_EQ(kAsync2, group2[1]->GetName());
  EXPECT_EQ("2", group2[0]->GetId());
  EXPECT_EQ(group2[0]->GetId(), group2[1]->GetId());
  EXPECT_EQ(TRACE_EVENT_PHASE_ASYNC_BEGIN, group2[0]->GetPhase());
  EXPECT_EQ(TRACE_EVENT_PHASE_ASYNC_END, group2[1]->GetPhase());
  EXPECT_EQ(1200000UL, group2[0]->GetTimestamp());
  EXPECT_EQ(1400000UL, group2[1]->GetTimestamp());
}

TEST_F(ArcTracingModelTest, InputEvents) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath tracing_path =
      base_path.Append("arc_graphics_tracing").Append("trace_input.dat.gz");

  std::string tracing_data_compressed;
  ASSERT_TRUE(base::ReadFileToString(tracing_path, &tracing_data_compressed));

  std::string tracing_data;
  ASSERT_TRUE(
      compression::GzipUncompress(tracing_data_compressed, &tracing_data));

  ArcTracingModel model;
  ASSERT_TRUE(model.Build(tracing_data));

  ArcTracingGraphicsModel graphics_model;
  graphics_model.set_skip_structure_validation();
  ASSERT_TRUE(graphics_model.Build(model));

  const std::vector<GraphicsEvents>& buffers =
      graphics_model.input().buffer_events();
  ASSERT_TRUE(buffers.size());

  for (const GraphicsEvents& buffer : buffers) {
    ASSERT_FALSE(buffer.empty());

    uint64_t last_timestamp = buffer[0].timestamp;
    GraphicsEventType last_type = buffer[0].type;
    EXPECT_EQ(GraphicsEventType::kInputEventCreated, last_type);
    for (size_t i = 1; i < buffer.size(); ++i) {
      const uint64_t timestamp = buffer[i].timestamp;
      const GraphicsEventType type = buffer[i].type;
      EXPECT_GE(timestamp, last_timestamp);
      // One input sequence may contain multiple input events.
      switch (last_type) {
        case GraphicsEventType::kInputEventCreated:
        case GraphicsEventType::kInputEventWaylandDispatched:
          EXPECT_TRUE(type == GraphicsEventType::kInputEventCreated ||
                      type == GraphicsEventType::kInputEventWaylandDispatched ||
                      type == GraphicsEventType::kInputEventDeliverStart);
          break;
        case GraphicsEventType::kInputEventDeliverStart:
          EXPECT_EQ(GraphicsEventType::kInputEventDeliverEnd, type);
          break;
        case GraphicsEventType::kInputEventDeliverEnd:
          EXPECT_EQ(GraphicsEventType::kInputEventCreated, type);
          break;
        default:
          NOTREACHED();
      }

      last_timestamp = timestamp;
      last_type = type;
    }
    EXPECT_EQ(GraphicsEventType::kInputEventDeliverEnd, last_type);
  }
}

}  // namespace arc
