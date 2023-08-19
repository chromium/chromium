// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/two_phase_uploader.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/safe_browsing/local_two_phase_testserver.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

class Delegate {
 public:
  Delegate() : state_(TwoPhaseUploader::STATE_NONE) {}

  void FinishCallback(base::RunLoop* run_loop,
                      TwoPhaseUploader::State state,
                      int net_error,
                      int response_code,
                      const std::string& response) {
    state_ = state;
    net_error_ = net_error;
    response_code_ = response_code;
    response_ = response;
    run_loop->Quit();
  }

  TwoPhaseUploader::State state_;
  int net_error_;
  int response_code_;
  std::string response_;
};

base::FilePath GetTestFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.AppendASCII("net");
  file_path = file_path.AppendASCII("data");
  file_path = file_path.AppendASCII("url_request_unittest");
  file_path = file_path.AppendASCII("BullRunSpeech.txt");
  return file_path;
}

}  // namespace

class TwoPhaseUploaderTest : public testing::Test {
 public:
  TwoPhaseUploaderTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    // Make sure the Network Service is started before making a NetworkContext.
    content::GetNetworkService();
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            network::NetworkService::GetNetworkServiceForTesting());

    // A NetworkContextClient is needed for uploads to work.
    mojo::PendingRemote<network::mojom::NetworkContextClient>
        network_context_client_remote;
    network_context_client_ =
        std::make_unique<network::TestNetworkContextClient>(
            network_context_client_remote.InitWithNewPipeAndPassReceiver());
    shared_url_loader_factory_->network_context()->SetClient(
        std::move(network_context_client_remote));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  std::unique_ptr<network::mojom::NetworkContextClient> network_context_client_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(TwoPhaseUploaderTest, UploadFile) {
  base::RunLoop run_loop;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  std::unique_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      shared_url_loader_factory_, task_runner_.get(),
      test_server.GetURL("/start"), "metadata", GetTestFilePath(),
      base::BindOnce(&Delegate::FinishCallback, base::Unretained(&delegate),
                     &run_loop),
      TRAFFIC_ANNOTATION_FOR_TESTS));
  uploader->Start();
  run_loop.Run();
  EXPECT_EQ(TwoPhaseUploader::STATE_SUCCESS, delegate.state_);
  EXPECT_EQ(net::OK, delegate.net_error_);
  EXPECT_EQ(200, delegate.response_code_);
  EXPECT_EQ(
      "/start\n"                                     // path of start request
      "4c24b2612e94e2ae622e54397663f2b7bf0a2e17\n"   // sha1sum of "metadata"
      "944857cc626f2cafe232521986b4c6d3f9993c97\n",  // sha1sum of test file
      delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, BadPhaseOneResponse) {
  base::RunLoop run_loop;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  std::unique_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      shared_url_loader_factory_, task_runner_.get(),
      test_server.GetURL("/start?p1code=500"), "metadata", GetTestFilePath(),
      base::BindOnce(&Delegate::FinishCallback, base::Unretained(&delegate),
                     &run_loop),
      TRAFFIC_ANNOTATION_FOR_TESTS));
  uploader->Start();
  run_loop.Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_METADATA, delegate.state_);
  EXPECT_EQ(net::OK, delegate.net_error_);
  EXPECT_EQ(500, delegate.response_code_);
  EXPECT_EQ("", delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, BadPhaseTwoResponse) {
  base::RunLoop run_loop;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  std::unique_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      shared_url_loader_factory_, task_runner_.get(),
      test_server.GetURL("/start?p2code=500"), "metadata", GetTestFilePath(),
      base::BindOnce(&Delegate::FinishCallback, base::Unretained(&delegate),
                     &run_loop),
      TRAFFIC_ANNOTATION_FOR_TESTS));
  uploader->Start();
  run_loop.Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_FILE, delegate.state_);
  EXPECT_EQ(net::OK, delegate.net_error_);
  EXPECT_EQ(500, delegate.response_code_);
  EXPECT_EQ(
      "/start\n"                                     // path of start request
      "4c24b2612e94e2ae622e54397663f2b7bf0a2e17\n"   // sha1sum of "metadata"
      "944857cc626f2cafe232521986b4c6d3f9993c97\n",  // sha1sum of test file
      delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, PhaseOneConnectionClosed) {
  base::RunLoop run_loop;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  std::unique_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      shared_url_loader_factory_, task_runner_.get(),
      test_server.GetURL("/start?p1close=1"), "metadata", GetTestFilePath(),
      base::BindOnce(&Delegate::FinishCallback, base::Unretained(&delegate),
                     &run_loop),
      TRAFFIC_ANNOTATION_FOR_TESTS));
  uploader->Start();
  run_loop.Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_METADATA, delegate.state_);
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, delegate.net_error_);
  EXPECT_EQ("", delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, PhaseTwoConnectionClosed) {
  base::RunLoop run_loop;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  std::unique_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      shared_url_loader_factory_, task_runner_.get(),
      test_server.GetURL("/start?p2close=1"), "metadata", GetTestFilePath(),
      base::BindOnce(&Delegate::FinishCallback, base::Unretained(&delegate),
                     &run_loop),
      TRAFFIC_ANNOTATION_FOR_TESTS));
  uploader->Start();
  run_loop.Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_FILE, delegate.state_);
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, delegate.net_error_);
  EXPECT_EQ("", delegate.response_);
}

}  // namespace safe_browsing
