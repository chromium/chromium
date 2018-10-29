// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_request_job.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_features.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/fake_file_system.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "google_apis/drive/test_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

// A simple URLRequestJobFactory implementation to create
// ExternalFileURLRequestJob.
class TestURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  explicit TestURLRequestJobFactory(void* profile_id)
      : profile_id_(profile_id) {}

  ~TestURLRequestJobFactory() override {}

  // net::URLRequestJobFactory override:
  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new ExternalFileURLRequestJob(
        profile_id_, request, network_delegate);
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return nullptr;
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    return scheme == content::kExternalFileScheme;
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return true;
  }

 private:
  void* const profile_id_;
  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJobFactory);
};

class TestDelegate : public net::TestDelegate {
 public:
  TestDelegate() {}

  const GURL& redirect_url() const { return redirect_url_; }

  // net::TestDelegate override.
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    redirect_url_ = redirect_info.new_url;
    net::TestDelegate::OnReceivedRedirect(request, redirect_info,
                                          defer_redirect);
  }

 private:
  GURL redirect_url_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

constexpr char kExtensionId[] = "abc";
constexpr char kFileSystemId[] = "test-filesystem";
constexpr char kTestUrl[] = "externalfile:abc:test-filesystem:/hello.txt";
constexpr char kExpectedFileContents[] =
    "This is a testing file. Lorem ipsum dolor sit amet est.";

}  // namespace

class ExternalFileURLRequestJobTest : public testing::Test {
 protected:
  ExternalFileURLRequestJobTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        integration_service_factory_callback_(base::Bind(
            &ExternalFileURLRequestJobTest::CreateDriveIntegrationService,
            base::Unretained(this))),
        fake_file_system_(NULL) {}

  ~ExternalFileURLRequestJobTest() override {}

  void SetUp() override {
    // Create a testing profile.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    Profile* const profile =
        profile_manager_->CreateTestingProfile("test-user");

    auto* service = chromeos::file_system_provider::Service::Get(profile);
    service->RegisterProvider(
        chromeos::file_system_provider::FakeExtensionProvider::Create(
            kExtensionId));
    const auto kProviderId =
        chromeos::file_system_provider::ProviderId::CreateFromExtensionId(
            kExtensionId);
    service->MountFileSystem(kProviderId,
                             chromeos::file_system_provider::MountOptions(
                                 kFileSystemId, "Test FileSystem"));

    // Create the drive integration service for the profile.
    integration_service_factory_scope_.reset(
        new drive::DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &integration_service_factory_callback_));
    drive::DriveIntegrationServiceFactory::GetForProfile(profile);

    // Create the URL request job factory.
    test_network_delegate_.reset(new net::TestNetworkDelegate);
    test_url_request_job_factory_.reset(new TestURLRequestJobFactory(profile));
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_job_factory(test_url_request_job_factory_.get());
    url_request_context_->set_network_delegate(test_network_delegate_.get());
    test_delegate_.reset(new TestDelegate);
  }

  void TearDown() override { profile_manager_.reset(); }

  bool ReadDriveFileSync(const base::FilePath& file_path,
                         std::string* out_content) {
    std::unique_ptr<base::Thread> worker_thread(
        new base::Thread("ReadDriveFileSync"));
    if (!worker_thread->Start())
      return false;

    std::unique_ptr<drive::DriveFileStreamReader> reader(
        new drive::DriveFileStreamReader(
            base::Bind(&ExternalFileURLRequestJobTest::GetFileSystem,
                       base::Unretained(this)),
            worker_thread->task_runner().get()));
    int error = net::ERR_FAILED;
    std::unique_ptr<drive::ResourceEntry> entry;
    {
      base::RunLoop run_loop;
      reader->Initialize(file_path,
                         net::HttpByteRange(),
                         google_apis::test_util::CreateQuitCallback(
                             &run_loop,
                             google_apis::test_util::CreateCopyResultCallback(
                                 &error, &entry)));
      run_loop.Run();
    }
    if (error != net::OK || !entry)
      return false;

    // Read data from the reader.
    std::string content;
    if (drive::test_util::ReadAllData(reader.get(), &content) != net::OK)
      return false;

    if (static_cast<size_t>(entry->file_info().size()) != content.size())
      return false;

    *out_content = content;
    return true;
  }

  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<TestDelegate> test_delegate_;

 private:
  // Create the drive integration service for the |profile|
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    drive::FakeDriveService* const drive_service = new drive::FakeDriveService;
    if (!drive::test_util::SetUpTestEntries(drive_service))
      return NULL;

    const std::string& drive_mount_name =
        drive::util::GetDriveMountPointPath(profile).BaseName().AsUTF8Unsafe();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        drive_mount_name,
        storage::kFileSystemTypeDrive,
        storage::FileSystemMountOption(),
        drive::util::GetDriveMountPointPath(profile));
    DCHECK(!fake_file_system_);
    fake_file_system_ = new drive::test_util::FakeFileSystem(drive_service);
    if (!drive_cache_dir_.CreateUniqueTempDir())
      return NULL;
    return new drive::DriveIntegrationService(
        profile, nullptr, drive_service, drive_mount_name,
        drive_cache_dir_.GetPath(), fake_file_system_);
  }

  drive::FileSystemInterface* GetFileSystem() { return fake_file_system_; }

  content::TestBrowserThreadBundle thread_bundle_;
  content::TestServiceManagerContext context_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      integration_service_factory_callback_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      integration_service_factory_scope_;
  std::unique_ptr<drive::DriveIntegrationService> integration_service_;
  drive::test_util::FakeFileSystem* fake_file_system_;

  std::unique_ptr<net::TestNetworkDelegate> test_network_delegate_;
  std::unique_ptr<TestURLRequestJobFactory> test_url_request_job_factory_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::ScopedTempDir drive_cache_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(ExternalFileURLRequestJobTest, NonGetMethod) {
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL(kTestUrl), net::DEFAULT_PRIORITY, test_delegate_.get()));
  request->set_method("POST");  // Set non "GET" method.
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED, test_delegate_->request_status());
}

TEST_F(ExternalFileURLRequestJobTest, RegularFile) {
  {
    std::unique_ptr<net::URLRequest> request(
        url_request_context_->CreateRequest(
            GURL(kTestUrl), net::DEFAULT_PRIORITY, test_delegate_.get()));
    request->Start();

    base::RunLoop().Run();

    ASSERT_EQ(net::OK, test_delegate_->request_status());
    std::string mime_type;
    request->GetMimeType(&mime_type);
    EXPECT_EQ("text/plain", mime_type);

    EXPECT_EQ(kExpectedFileContents, test_delegate_->data_received());
  }
}

TEST_F(ExternalFileURLRequestJobTest, HostedDocument) {
  // Hosted documents are never opened via externalfile: URLs with DriveFS.
  if (base::FeatureList::IsEnabled(chromeos::features::kDriveFs)) {
    return;
  }
  // Open a gdoc file.
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:drive-test-user-hash/root/Document 1 "
           "excludeDir-test.gdoc"),
      net::DEFAULT_PRIORITY, test_delegate_.get()));
  request->Start();

  test_delegate_->RunUntilRedirect();

  // Make sure that a hosted document triggers redirection.
  EXPECT_TRUE(request->is_redirecting());
  EXPECT_TRUE(test_delegate_->redirect_url().is_valid());
}

TEST_F(ExternalFileURLRequestJobTest, RootDirectory) {
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:abc:test-filesystem:/"), net::DEFAULT_PRIORITY,
      test_delegate_.get()));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::ERR_FAILED, test_delegate_->request_status());
}

TEST_F(ExternalFileURLRequestJobTest, NonExistingFile) {
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:abc:test-filesystem:/non-existing-file.txt"),
      net::DEFAULT_PRIORITY, test_delegate_.get()));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, test_delegate_->request_status());
}

TEST_F(ExternalFileURLRequestJobTest, WrongFormat) {
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL("externalfile:"), net::DEFAULT_PRIORITY, test_delegate_.get()));
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::ERR_INVALID_URL, test_delegate_->request_status());
}

TEST_F(ExternalFileURLRequestJobTest, Cancel) {
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL(kTestUrl), net::DEFAULT_PRIORITY, test_delegate_.get()));

  // Start the request, and cancel it immediately after it.
  request->Start();
  request->Cancel();

  base::RunLoop().Run();

  EXPECT_EQ(net::ERR_ABORTED, test_delegate_->request_status());
}

TEST_F(ExternalFileURLRequestJobTest, RangeHeader) {
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL(kTestUrl), net::DEFAULT_PRIORITY, test_delegate_.get()));

  // Set range header.
  request->SetExtraRequestHeaderByName(
      "Range", "bytes=3-5", false /* overwrite */);
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::OK, test_delegate_->request_status());

  EXPECT_EQ(base::StringPiece(kExpectedFileContents).substr(3, 3),
            test_delegate_->data_received());
}

TEST_F(ExternalFileURLRequestJobTest, WrongRangeHeader) {
  std::unique_ptr<net::URLRequest> request(url_request_context_->CreateRequest(
      GURL(kTestUrl), net::DEFAULT_PRIORITY, test_delegate_.get()));

  // Set range header.
  request->SetExtraRequestHeaderByName(
      "Range", "Wrong Range Header Value", false /* overwrite */);
  request->Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            test_delegate_->request_status());
}

}  // namespace chromeos
