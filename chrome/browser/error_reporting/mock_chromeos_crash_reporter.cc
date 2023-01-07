// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple binary that replicates the crash_reporter / crash_sender
// functionality of Chrome OS for testing purposes. In particular, it has a
// stripped-down version of the parsing logic in
// src/platform2/crash-reporter/chrome_collector.cc, coupled with a simple
// upload function similar to src/platform2/crash-reporter/crash_sender_util.cc
// (but without the compression). This is used in tests to substitute for the
// actual OS crash reporting system.

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/error_reporting/constants.h"
#include "net/http/http_status_code.h"
#include "third_party/crashpad/crashpad/third_party/cpp-httplib/cpp-httplib/httplib.h"
#include "url/gurl.h"

namespace {

// Parses the key:length:value triplets similar to
// ChromeCollector::ParseCrashLog. Input is the file descriptor |fd|,
// return is a list of key/value pairs in |values| and a payload in |payload|.
//
// Closes |fd| when done.
bool ParseTriplets(int fd,
                   std::map<std::string, std::string>& values,
                   std::string& payload) {
  base::ScopedFILE stream(fdopen(fd, "rb"));
  if (!stream.get()) {
    PLOG(ERROR) << "fdopen failed!";
    return false;
  }
  std::string data;
  if (!base::ReadStreamToString(stream.get(), &data)) {
    LOG(WARNING) << "Read failed!";
  }

  std::string::size_type pos = 0;
  while (pos < data.size()) {
    std::string::size_type end_of_key = data.find(':', pos);
    if (end_of_key == std::string::npos) {
      LOG(ERROR) << "Incomplete value found, starting at position " << pos;
      return false;
    }

    std::string key = data.substr(pos, end_of_key - pos);
    std::string::size_type end_of_length = data.find(':', end_of_key + 1);
    if (end_of_length == std::string::npos) {
      LOG(ERROR) << "Incomplete length found, starting at position "
                 << (end_of_key + 1);
      return false;
    }

    std::string length_string =
        data.substr(end_of_key + 1, end_of_length - (end_of_key + 1));
    size_t length;
    if (!base::StringToSizeT(length_string, &length)) {
      LOG(ERROR) << "Bad length string '" << length_string << "'";
      return false;
    }

    std::string value = data.substr(end_of_length + 1, length);
    pos = end_of_length + length + 1;

    if (key == kJavaScriptStackKey) {
      payload = std::move(value);
    } else {
      values.emplace(std::move(key), std::move(value));
    }
  }
  return true;
}

// Upload the error report to the provided URL.
bool UploadViaHttp(const std::string& base_url,
                   const std::map<std::string, std::string>& values,
                   const std::string& payload) {
  std::vector<std::string> query_parts;
  for (const auto& kv : values) {
    query_parts.emplace_back(base::StrCat(
        {base::EscapeQueryParamValue(kv.first, /*use_plus=*/false), "=",
         base::EscapeQueryParamValue(kv.second, /*use_plus=*/false)}));
  }
  std::string upload_str =
      base::StrCat({base_url, "?", base::JoinString(query_parts, "&")});

  GURL upload_url(upload_str);
  if (!upload_url.is_valid()) {
    LOG(ERROR) << "Invalid upload_to URL: '" << upload_str << "'";
    return false;
  }

  // Upload using httplib. The normal Chromium way (SimpleURLLoader) needs a lot
  // of browser stuff to be set up before it can be used, so we use the
  // standalone httplib in this test binary.
  std::string host = upload_url.host();
  httplib::Client cli(host.c_str(), upload_url.EffectiveIntPort());
  if (!cli.is_valid()) {
    LOG(ERROR) << "httplib::Client setup error";
    return false;
  }

  std::string path = upload_url.PathForRequest();
  auto response = cli.Post(path.c_str(), payload, "text/plain");
  if (!response) {
    LOG(ERROR) << "No response to Post";
    return false;
  }

  if (response->status != net::HTTP_OK) {
    LOG(ERROR) << "http response " << response->status;
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  constexpr char kFdSwitch[] = "chrome_memfd";
  if (!cmd_line->HasSwitch(kFdSwitch)) {
    LOG(ERROR) << "No --chrome_memfd";
    return EXIT_FAILURE;
  }
  auto fd_string = cmd_line->GetSwitchValueASCII(kFdSwitch);
  int fd;
  if (!base::StringToInt(fd_string, &fd)) {
    LOG(ERROR) << "Can't parse --chrome_memfd '" << fd_string << "' as int";
    return EXIT_FAILURE;
  }

  // Note: This must be a map (not an unordered_map or such) because some unit
  // tests rely on the order of the parameters in the URL string. Until that's
  // fixed, keep the values sorted by key in the URL.
  std::map<std::string, std::string> values;
  std::string payload;
  if (!ParseTriplets(fd, values, payload)) {
    return EXIT_FAILURE;
  }

  constexpr char kUploadSwitch[] = "upload_to";
  if (!cmd_line->HasSwitch(kUploadSwitch)) {
    LOG(ERROR) << "No --upload_to";
    return EXIT_FAILURE;
  }
  std::string base_url = cmd_line->GetSwitchValueASCII(kUploadSwitch);
  if (!UploadViaHttp(base_url, values, payload)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
