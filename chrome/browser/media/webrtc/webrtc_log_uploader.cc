// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"

#include <stddef.h>
#include <cstdlib>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/version_info/version_info.h"
#include "components/webrtc_logging/browser/log_cleanup.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "components/webrtc_logging/common/partial_circular_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/zlib/zlib.h"

namespace {

const int kLogCountLimit = 5;
const uint32_t kIntermediateCompressionBufferBytes = 256 * 1024;  // 256 KB
const int kLogListLimitLines = 50;

const char kWebrtcLogUploadContentType[] = "multipart/form-data";
const char kWebrtcLogMultipartBoundary[] =
    "----**--yradnuoBgoLtrapitluMklaTelgooG--**----";

// Adds the header section for a gzip file to the multipart |post_data|.
void AddMultipartFileContentHeader(std::string* post_data,
                                   const std::string& content_name) {
  post_data->append("--");
  post_data->append(kWebrtcLogMultipartBoundary);
  post_data->append("\r\nContent-Disposition: form-data; name=\"");
  post_data->append(content_name);
  post_data->append("\"; filename=\"");
  post_data->append(content_name + ".gz");
  post_data->append("\"\r\nContent-Type: application/gzip\r\n\r\n");
}

// Adds |compressed_log| to |post_data|.
void AddLogData(std::string* post_data, const std::string& compressed_log) {
  AddMultipartFileContentHeader(post_data, "webrtc_log");
  post_data->append(compressed_log);
  post_data->append("\r\n");
}

// Adds the RTP dump data to |post_data|.
void AddRtpDumpData(std::string* post_data,
                    const std::string& name,
                    const std::string& dump_data) {
  AddMultipartFileContentHeader(post_data, name);
  post_data->append(dump_data.data(), dump_data.size());
  post_data->append("\r\n");
}

// Helper for WebRtcLogUploader::CompressLog().
void ResizeForNextOutput(std::string* compressed_log, z_stream* stream) {
  size_t old_size = compressed_log->size() - stream->avail_out;
  compressed_log->resize(old_size + kIntermediateCompressionBufferBytes);
  stream->next_out =
      reinterpret_cast<unsigned char*>(&(*compressed_log)[old_size]);
  stream->avail_out = kIntermediateCompressionBufferBytes;
}

}  // namespace

WebRtcLogUploader::UploadDoneData::UploadDoneData() = default;
WebRtcLogUploader::UploadDoneData::UploadDoneData(
    const WebRtcLogUploader::UploadDoneData& other) = default;
WebRtcLogUploader::UploadDoneData::~UploadDoneData() = default;

WebRtcLogUploader::WebRtcLogUploader()
    : main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      background_task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::BEST_EFFORT})) {}

WebRtcLogUploader::~WebRtcLogUploader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(pending_uploads_.empty());
  DCHECK(shutdown_);
}

bool WebRtcLogUploader::ApplyForStartLogging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (log_count_ < kLogCountLimit && !shutdown_) {
    ++log_count_;
    return true;
  }
  return false;
}

void WebRtcLogUploader::LoggingStoppedDontUpload() {
  DecreaseLogCount();
}

void WebRtcLogUploader::LoggingStoppedDoUpload(
    std::unique_ptr<WebRtcLogBuffer> log_buffer,
    std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
    const WebRtcLogUploader::UploadDoneData& upload_done_data) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(log_buffer.get());
  DCHECK(meta_data.get());
  DCHECK(!upload_done_data.paths.directory.empty());

  std::string compressed_log = CompressLog(log_buffer.get());

  std::string local_log_id;

  if (base::PathExists(upload_done_data.paths.directory)) {
    webrtc_logging::DeleteOldWebRtcLogFiles(upload_done_data.paths.directory);

    local_log_id = base::NumberToString(base::Time::Now().ToDoubleT());
    base::FilePath log_file_path =
        upload_done_data.paths.directory.AppendASCII(local_log_id)
            .AddExtension(FILE_PATH_LITERAL(".gz"));
    WriteCompressedLogToFile(compressed_log, log_file_path);

    base::FilePath log_list_path =
        webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(
            upload_done_data.paths.directory);
    AddLocallyStoredLogInfoToUploadListFile(log_list_path, local_log_id);
  }

  UploadDoneData upload_done_data_with_log_id = upload_done_data;
  upload_done_data_with_log_id.local_log_id = local_log_id;
  PrepareMultipartPostData(compressed_log, std::move(meta_data),
                           upload_done_data_with_log_id);
}

void WebRtcLogUploader::PrepareMultipartPostData(
    const std::string& compressed_log,
    std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
    const WebRtcLogUploader::UploadDoneData& upload_done_data) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!compressed_log.empty());
  DCHECK(meta_data.get());

  std::unique_ptr<std::string> post_data(new std::string());
  SetupMultipart(post_data.get(), compressed_log,
                 upload_done_data.paths.incoming_rtp_dump,
                 upload_done_data.paths.outgoing_rtp_dump, *meta_data.get());

  // If a test has set the test string pointer, write to it and skip uploading.
  // Still fire the upload callback so that we can run an extension API test
  // using the test framework for that without hanging.
  // TODO(grunell): Remove this when the api test for this feature is fully
  // implemented according to the test plan. http://crbug.com/257329.
  if (post_data_) {
    *post_data_ = *post_data;
    NotifyUploadDoneAndLogStats(net::HTTP_OK, net::OK, "", upload_done_data);
    return;
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcLogUploader::UploadCompressedLog,
                                base::Unretained(this), upload_done_data,
                                std::move(post_data)));
}

void WebRtcLogUploader::UploadStoredLog(
    const WebRtcLogUploader::UploadDoneData& upload_data) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!upload_data.local_log_id.empty());
  DCHECK(!upload_data.paths.directory.empty());

  base::FilePath native_log_path =
      upload_data.paths.directory.AppendASCII(upload_data.local_log_id)
          .AddExtension(FILE_PATH_LITERAL(".gz"));

  std::string compressed_log;
  if (!base::ReadFileToString(native_log_path, &compressed_log)) {
    DPLOG(WARNING) << "Could not read WebRTC log file.";
    base::UmaHistogramSparse("WebRtcTextLogging.UploadFailed",
                             upload_data.web_app_id);
    base::UmaHistogramSparse("WebRtcTextLogging.UploadFailureReason",
                             WebRtcLogUploadFailureReason::kStoredLogNotFound);
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(upload_data.callback, false, "", "Log doesn't exist."));
    return;
  }

  UploadDoneData upload_data_with_rtp = upload_data;

  // Optimistically set the rtp paths to what they should be if they exist.
  upload_data_with_rtp.paths.incoming_rtp_dump =
      upload_data.paths.directory.AppendASCII(upload_data.local_log_id)
          .AddExtension(FILE_PATH_LITERAL(".rtp_in"));

  upload_data_with_rtp.paths.outgoing_rtp_dump =
      upload_data.paths.directory.AppendASCII(upload_data.local_log_id)
          .AddExtension(FILE_PATH_LITERAL(".rtp_out"));

  std::unique_ptr<WebRtcLogMetaDataMap> meta_data(new WebRtcLogMetaDataMap());
  {
    std::string meta_data_contents;
    base::FilePath meta_path =
        upload_data.paths.directory.AppendASCII(upload_data.local_log_id)
            .AddExtension(FILE_PATH_LITERAL(".meta"));
    if (base::ReadFileToString(meta_path, &meta_data_contents) &&
        !meta_data_contents.empty()) {
      base::Pickle pickle(&meta_data_contents[0], meta_data_contents.size());
      base::PickleIterator it(pickle);
      std::string key, value;
      while (it.ReadString(&key) && it.ReadString(&value))
        (*meta_data.get())[key] = value;
    }
  }

  PrepareMultipartPostData(compressed_log, std::move(meta_data),
                           upload_data_with_rtp);
}

void WebRtcLogUploader::LoggingStoppedDoStore(
    const WebRtcLogPaths& log_paths,
    const std::string& log_id,
    std::unique_ptr<WebRtcLogBuffer> log_buffer,
    std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
    const GenericDoneCallback& done_callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!log_id.empty());
  DCHECK(log_buffer.get());
  DCHECK(!log_paths.directory.empty());

  webrtc_logging::DeleteOldWebRtcLogFiles(log_paths.directory);

  base::FilePath log_list_path =
      webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(
          log_paths.directory);

  // Store the native log with a ".gz" extension.
  std::string compressed_log = CompressLog(log_buffer.get());
  base::FilePath native_log_path =
      log_paths.directory.AppendASCII(log_id).AddExtension(
          FILE_PATH_LITERAL(".gz"));
  WriteCompressedLogToFile(compressed_log, native_log_path);
  AddLocallyStoredLogInfoToUploadListFile(log_list_path, log_id);

  // Move the rtp dump files to the log directory with a name of
  // <log id>.rtp_[in|out].
  if (!log_paths.incoming_rtp_dump.empty()) {
    base::FilePath rtp_path =
        log_paths.directory.AppendASCII(log_id).AddExtension(
            FILE_PATH_LITERAL(".rtp_in"));
    base::Move(log_paths.incoming_rtp_dump, rtp_path);
  }

  if (!log_paths.outgoing_rtp_dump.empty()) {
    base::FilePath rtp_path =
        log_paths.directory.AppendASCII(log_id).AddExtension(
            FILE_PATH_LITERAL(".rtp_out"));
    base::Move(log_paths.outgoing_rtp_dump, rtp_path);
  }

  if (meta_data.get() && !meta_data->empty()) {
    base::Pickle pickle;
    for (const auto& it : *meta_data.get()) {
      pickle.WriteString(it.first);
      pickle.WriteString(it.second);
    }
    base::FilePath meta_path =
        log_paths.directory.AppendASCII(log_id).AddExtension(
            FILE_PATH_LITERAL(".meta"));
    base::WriteFile(meta_path, static_cast<const char*>(pickle.data()),
                    pickle.size());
  }

  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(done_callback, true, ""));

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcLogUploader::DecreaseLogCount,
                                base::Unretained(this)));
}

void WebRtcLogUploader::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(!shutdown_);

  // Clear the pending uploads list, which will reset all URL loaders.
  pending_uploads_.clear();
  shutdown_ = true;
}

void WebRtcLogUploader::OnSimpleLoaderComplete(
    SimpleURLLoaderList::iterator it,
    const WebRtcLogUploader::UploadDoneData& upload_done_data,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(!shutdown_);
  network::SimpleURLLoader* loader = it->get();
  base::Optional<int> response_code;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }
  const int network_error_code = loader->NetError();
  pending_uploads_.erase(it);
  std::string report_id;
  if (response_body)
    report_id = std::move(*response_body);
  // The log path can be empty here if we failed getting it before. We still
  // upload the log if that's the case.
  if (!upload_done_data.paths.directory.empty()) {
    // TODO(jiayl): Add the RTP dump records to chrome://webrtc-logs.
    base::FilePath log_list_path =
        webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(
            upload_done_data.paths.directory);
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcLogUploader::AddUploadedLogInfoToUploadListFile,
                       log_list_path, upload_done_data.local_log_id,
                       report_id));
  }
  NotifyUploadDoneAndLogStats(response_code, network_error_code, report_id,
                              upload_done_data);
}

void WebRtcLogUploader::SetupMultipart(
    std::string* post_data,
    const std::string& compressed_log,
    const base::FilePath& incoming_rtp_dump,
    const base::FilePath& outgoing_rtp_dump,
    const std::map<std::string, std::string>& meta_data) {
#if defined(OS_WIN)
  const char product[] = "Chrome";
#elif defined(OS_MACOSX)
  const char product[] = "Chrome_Mac";
#elif defined(OS_LINUX)
#if !defined(ADDRESS_SANITIZER)
  const char product[] = "Chrome_Linux";
#else
  const char product[] = "Chrome_Linux_ASan";
#endif
#elif defined(OS_ANDROID)
  const char product[] = "Chrome_Android";
#elif defined(OS_CHROMEOS)
  const char product[] = "Chrome_ChromeOS";
#else
#error Platform not supported.
#endif
  net::AddMultipartValueForUpload("prod", product, kWebrtcLogMultipartBoundary,
                                  "", post_data);
  net::AddMultipartValueForUpload("ver",
                                  version_info::GetVersionNumber() + "-webrtc",
                                  kWebrtcLogMultipartBoundary, "", post_data);
  net::AddMultipartValueForUpload("guid", "0", kWebrtcLogMultipartBoundary, "",
                                  post_data);
  net::AddMultipartValueForUpload("type", "webrtc_log",
                                  kWebrtcLogMultipartBoundary, "", post_data);

  // Add custom meta data.
  for (const auto& it : meta_data) {
    net::AddMultipartValueForUpload(it.first, it.second,
                                    kWebrtcLogMultipartBoundary, "", post_data);
  }

  AddLogData(post_data, compressed_log);

  // Add the rtp dumps if they exist.
  base::FilePath rtp_dumps[2] = {incoming_rtp_dump, outgoing_rtp_dump};
  static const char* const kRtpDumpNames[2] = {"rtpdump_recv", "rtpdump_send"};

  for (size_t i = 0; i < 2; ++i) {
    if (!rtp_dumps[i].empty() && base::PathExists(rtp_dumps[i])) {
      std::string dump_data;
      if (base::ReadFileToString(rtp_dumps[i], &dump_data))
        AddRtpDumpData(post_data, kRtpDumpNames[i], dump_data);
    }
  }

  net::AddMultipartFinalDelimiterForUpload(kWebrtcLogMultipartBoundary,
                                           post_data);
}

std::string WebRtcLogUploader::CompressLog(WebRtcLogBuffer* buffer) {
  z_stream stream = {0};
  int result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                            // windowBits = 15 is default, 16 is added to
                            // produce a gzip header + trailer.
                            15 + 16,
                            8,  // memLevel = 8 is default.
                            Z_DEFAULT_STRATEGY);
  DCHECK_EQ(Z_OK, result);

  std::string compressed_log;
  ResizeForNextOutput(&compressed_log, &stream);

  uint8_t intermediate_buffer[kIntermediateCompressionBufferBytes] = {0};
  webrtc_logging::PartialCircularBuffer read_buffer(buffer->Read());
  do {
    if (stream.avail_in == 0) {
      uint32_t read = read_buffer.Read(&intermediate_buffer[0],
                                       sizeof(intermediate_buffer));
      stream.next_in = &intermediate_buffer[0];
      stream.avail_in = read;
      if (read != kIntermediateCompressionBufferBytes)
        break;
    }
    result = deflate(&stream, Z_SYNC_FLUSH);
    DCHECK_EQ(Z_OK, result);
    if (stream.avail_out == 0)
      ResizeForNextOutput(&compressed_log, &stream);
  } while (true);

  // Ensure we have enough room in the output buffer. Easier to always just do a
  // resize than looping around and resize if needed.
  if (stream.avail_out < kIntermediateCompressionBufferBytes)
    ResizeForNextOutput(&compressed_log, &stream);

  result = deflate(&stream, Z_FINISH);
  DCHECK_EQ(Z_STREAM_END, result);
  result = deflateEnd(&stream);
  DCHECK_EQ(Z_OK, result);

  compressed_log.resize(compressed_log.size() - stream.avail_out);
  return compressed_log;
}

void WebRtcLogUploader::UploadCompressedLog(
    const WebRtcLogUploader::UploadDoneData& upload_done_data,
    std::unique_ptr<std::string> post_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  DecreaseLogCount();

  // We don't log upload failure to UMA in case of shutting down for
  // consistency, since there are other cases during shutdown were we don't get
  // a chance to log.
  if (shutdown_)
    return;

  std::string content_type = kWebrtcLogUploadContentType;
  content_type.append("; boundary=");
  content_type.append(kWebrtcLogMultipartBoundary);

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("webrtc_log_upload", R"(
        semantics {
          sender: "Webrtc Log Uploader"
          description: "Uploads WebRTC debug logs for Hangouts."
          trigger:
            "When a Hangouts extension or Hangouts services extension signals "
            "to upload via the private WebRTC logging extension API."
          data:
            "WebRTC specific log entries, additional system information, and "
            "RTP packet headers for incoming and outgoing WebRTC streams. "
            "Audio or video data is never sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled by unchecking 'Report additional "
            "diagnostics to help improve Hangouts.' in Hangouts settings."
          policy_exception_justification:
            "Not implemented, it would be good to do so."
        })");

  constexpr char kUploadURL[] = "https://clients2.google.com/cr/report";
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = !upload_url_for_testing_.is_empty()
                              ? upload_url_for_testing_
                              : GURL(kUploadURL);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_url_loader->AttachStringForUpload(*post_data, content_type);
  auto it = pending_uploads_.insert(pending_uploads_.begin(),
                                    std::move(simple_url_loader));
  network::SimpleURLLoader* raw_loader = it->get();
  raw_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      g_browser_process->shared_url_loader_factory().get(),
      base::BindOnce(&WebRtcLogUploader::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(it), upload_done_data));
}

void WebRtcLogUploader::DecreaseLogCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  --log_count_;
}

void WebRtcLogUploader::WriteCompressedLogToFile(
    const std::string& compressed_log,
    const base::FilePath& log_file_path) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!compressed_log.empty());
  base::WriteFile(log_file_path, &compressed_log[0], compressed_log.size());
}

void WebRtcLogUploader::AddLocallyStoredLogInfoToUploadListFile(
    const base::FilePath& upload_list_path,
    const std::string& local_log_id) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!upload_list_path.empty());
  DCHECK(!local_log_id.empty());

  std::string contents;

  if (base::PathExists(upload_list_path)) {
    if (!base::ReadFileToString(upload_list_path, &contents)) {
      DPLOG(WARNING) << "Could not read WebRTC log list file.";
      return;
    }

    // Limit the number of log entries to |kLogListLimitLines| - 1, to make room
    // for the new entry. Each line including the last ends with a '\n', so hit
    // n will be before line n-1 (from the back).
    int lf_count = 0;
    int i = contents.size() - 1;
    for (; i >= 0 && lf_count < kLogListLimitLines; --i) {
      if (contents[i] == '\n')
        ++lf_count;
    }
    if (lf_count >= kLogListLimitLines) {
      // + 1 to compensate for the for loop decrease before the conditional
      // check and + 1 to get the length.
      contents.erase(0, i + 2);
    }
  }

  // Write the log ID and capture time to the log list file. Leave the upload
  // time and report ID empty.
  contents += ",," + local_log_id + "," +
              base::NumberToString(base::Time::Now().ToDoubleT()) + '\n';

  int written =
      base::WriteFile(upload_list_path, &contents[0], contents.size());
  if (written != static_cast<int>(contents.size())) {
    DPLOG(WARNING) << "Could not write all data to WebRTC log list file: "
                   << written;
  }
}

// static
void WebRtcLogUploader::AddUploadedLogInfoToUploadListFile(
    const base::FilePath& upload_list_path,
    const std::string& local_log_id,
    const std::string& report_id) {
  DCHECK(!upload_list_path.empty());
  DCHECK(!local_log_id.empty());
  DCHECK(!report_id.empty());

  std::string contents;

  if (base::PathExists(upload_list_path)) {
    if (!base::ReadFileToString(upload_list_path, &contents)) {
      DPLOG(WARNING) << "Could not read WebRTC log list file.";
      return;
    }
  }

  // Write the Unix time and report ID to the log list file. We should be able
  // to find the local log ID, in that case insert the data into the existing
  // line. Otherwise add it in the end.
  base::Time time_now = base::Time::Now();
  std::string time_now_str = base::NumberToString(time_now.ToDoubleT());
  size_t pos = contents.find(",," + local_log_id);
  if (pos != std::string::npos) {
    contents.insert(pos, time_now_str);
    contents.insert(pos + time_now_str.length() + 1, report_id);
  } else {
    contents += time_now_str + "," + report_id + ",," + time_now_str + "\n";
  }

  int written =
      base::WriteFile(upload_list_path, &contents[0], contents.size());
  if (written != static_cast<int>(contents.size())) {
    DPLOG(WARNING) << "Could not write all data to WebRTC log list file: "
                   << written;
  }
}

void WebRtcLogUploader::NotifyUploadDoneAndLogStats(
    base::Optional<int> response_code,
    int network_error_code,
    const std::string& report_id,
    const WebRtcLogUploader::UploadDoneData& upload_done_data) {
  if (upload_done_data.callback.is_null())
    return;

  const bool success = response_code == net::HTTP_OK;
  std::string error_message;
  if (success) {
    base::UmaHistogramSparse("WebRtcTextLogging.UploadSuccessful",
                             upload_done_data.web_app_id);
  } else {
    base::UmaHistogramSparse("WebRtcTextLogging.UploadFailed",
                             upload_done_data.web_app_id);
    if (response_code.has_value()) {
      base::UmaHistogramSparse("WebRtcTextLogging.UploadFailureReason",
                               response_code.value());
    } else {
      DCHECK_NE(network_error_code, net::OK);
      base::UmaHistogramSparse("WebRtcTextLogging.UploadFailureReason",
                               WebRtcLogUploadFailureReason::kNetworkError);
      base::UmaHistogramSparse("WebRtcTextLogging.UploadFailureNetErrorCode",
                               std::abs(network_error_code));
    }
    error_message = base::StrCat(
        {"Uploading failed, response code: ",
         response_code.has_value() ? base::NumberToString(response_code.value())
                                   : "<no value>"});
  }
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(upload_done_data.callback, success, report_id,
                                error_message));
}
