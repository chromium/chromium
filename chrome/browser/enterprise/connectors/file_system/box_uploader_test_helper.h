// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_TEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_TEST_HELPER_H_

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"
#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class BoxUploader;

using State = download::DownloadItem::DownloadState;

class BoxUploaderTestBase : public testing::Test {
 public:
  explicit BoxUploaderTestBase(base::FilePath::StringPieceType file_name =
                                   FILE_PATH_LITERAL("box_uploader_test.txt"));
  ~BoxUploaderTestBase() override;

  base::FilePath GetFilePath() const;

 protected:
  virtual void CreateTemporaryFile();
  void CreateTemporaryFileWithContent(std::string content);
  void InitFolderIdInPrefs(std::string folder_id);
  void InitUploader(BoxUploader* uploader);

  void AuthenticationRetry();
  void OnProgressUpdate(
      const download::DownloadItemRenameProgressUpdate& update);
  void OnUploaderFinished(download::DownloadInterruptReason reason,
                          const base::FilePath& final_name);
  void TearDown() override;

  // The following methods add mock responses to the |test_url_loader_factory_|
  // for http requests made to the specified url. Avoid mixing the use of
  // AddFetchResult() vs AddSequentialFetchResult() on the same url.

  // Add a repeating mock response. Only the last response added is used.
  void AddFetchResult(const std::string& url,
                      net::HttpStatusCode code,
                      std::string body = std::string());
  // Add multiple responses for the same url to be consumed in a FIFO sequence.
  void AddSequentialFetchResult(const std::string& url,
                                net::HttpStatusCode code,
                                std::string body = std::string());
  void AddSequentialFetchResult(const std::string& url,
                                network::mojom::URLResponseHeadPtr head,
                                std::string body = std::string());
  void ClearFetchResults(const std::string& url);
  size_t GetPendingSequentialResponsesCount(const std::string& url) const;

  // The following methods should be used to surround multi-threaded code block.
  // Use these wherever possible, instead of base::RunLoop().RunUntilIdle(),
  // which is flaky in multi-threaded environment.
  void InitQuitClosure();
  void RunWithQuitClosure();
  void Quit();

  DownloadItemForTest test_item_;

  // For uploader.TryTask().
  scoped_refptr<network::SharedURLLoaderFactory> url_factory_;

  // Updated/used in callbacks & checked in tests.
  int authentication_retry_{0};
  int progress_update_cb_called_{0};
  bool download_thread_cb_called_{false};
  bool upload_success_{false};
  download::DownloadInterruptReason reason_{
      download::DOWNLOAD_INTERRUPT_REASON_NONE};
  base::FilePath file_name_reported_back_;
  DownloadItemRerouteInfo reroute_info_reported_back_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;  // For url_factory_.
  // Decoder and TestingProfileManager must be declared after TaskEnvironment.
  data_decoder::test::InProcessDataDecoder decoder_;  // For parsing responses.
  TestingProfileManager profile_manager_;             // For prefs_.
  raw_ptr<PrefService> prefs_;                        // For storing folder_id.

  // For RunWithQuitClosure() and Quit().
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure quit_closure_;

  // Helper methods, struct, and member for AddSequentialFetchResult().
  void SetInterceptorForURLLoader(network::TestURLLoaderFactory::Interceptor);
  void SetNextResponseForURLLoader(const network::ResourceRequest& request);
  struct HttpResponse {
    HttpResponse(size_t idx,
                 network::mojom::URLResponseHeadPtr head,
                 std::string body);
    ~HttpResponse();
    HttpResponse(HttpResponse&&);

    size_t idx_;
    network::mojom::URLResponseHeadPtr head_;
    std::string body_;
  };
  size_t idx_sequential_add = 0;
  size_t idx_sequential_fetch = 0;
  std::multimap<GURL, HttpResponse> sequential_responses_;
  std::set<GURL> repeating_responses_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_TEST_HELPER_H_
