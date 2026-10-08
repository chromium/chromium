// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kExtensionIdA[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionIdB[] = "bcdefghijklmnopabcdefghijklmnopa";

TEST(MediaFileSystemBackendTest, ConstructAndParseMountName) {
  const base::FilePath profile_path(FILE_PATH_LITERAL("/profiles/Default"));
  const std::string mount_name = MediaFileSystemBackend::ConstructMountName(
      profile_path, kExtensionIdA, 42);
  EXPECT_EQ("media_galleries-Default-abcdefghijklmnopabcdefghijklmnop-42",
            mount_name);

  std::optional<MediaFileSystemBackend::ParsedMountName> parsed =
      MediaFileSystemBackend::ParseMountName(mount_name);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ("Default", parsed->profile_base_name);
  EXPECT_EQ(kExtensionIdA, parsed->extension_id);
  EXPECT_EQ(42u, parsed->pref_id);
}

TEST(MediaFileSystemBackendTest,
     ConstructAndParseMountNameWithHyphensAndSpacesInProfile) {
  const base::FilePath profile_path(
      FILE_PATH_LITERAL("/home/chronos/u-0123456789abcdef/Profile 1"));
  const std::string mount_name = MediaFileSystemBackend::ConstructMountName(
      profile_path, kExtensionIdA, 7);
  EXPECT_EQ("media_galleries-Profile_1-abcdefghijklmnopabcdefghijklmnop-7",
            mount_name);

  std::optional<MediaFileSystemBackend::ParsedMountName> parsed =
      MediaFileSystemBackend::ParseMountName(mount_name);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ("Profile_1", parsed->profile_base_name);
  EXPECT_EQ(kExtensionIdA, parsed->extension_id);
  EXPECT_EQ(7u, parsed->pref_id);

  // Profile base name containing hyphens.
  const base::FilePath hyphen_profile(
      FILE_PATH_LITERAL("/home/chronos/u-0123456789abcdef"));
  const std::string hyphen_mount = MediaFileSystemBackend::ConstructMountName(
      hyphen_profile, kExtensionIdA, 99);
  parsed = MediaFileSystemBackend::ParseMountName(hyphen_mount);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ("u-0123456789abcdef", parsed->profile_base_name);
  EXPECT_EQ(kExtensionIdA, parsed->extension_id);
  EXPECT_EQ(99u, parsed->pref_id);
}

TEST(MediaFileSystemBackendTest, ParseMountNameInvalidInputs) {
  // Empty string or wrong prefix.
  EXPECT_FALSE(MediaFileSystemBackend::ParseMountName("").has_value());
  EXPECT_FALSE(MediaFileSystemBackend::ParseMountName(
                   "other_prefix-Default-abcdefghijklmnopabcdefghijklmnop-1")
                   .has_value());
  EXPECT_FALSE(
      MediaFileSystemBackend::ParseMountName("media_galleries-").has_value());

  // Missing components.
  EXPECT_FALSE(MediaFileSystemBackend::ParseMountName("media_galleries-Default")
                   .has_value());
  EXPECT_FALSE(
      MediaFileSystemBackend::ParseMountName("media_galleries-Default-1")
          .has_value());

  // Empty profile or empty extension ID.
  EXPECT_FALSE(MediaFileSystemBackend::ParseMountName(
                   "media_galleries--abcdefghijklmnopabcdefghijklmnop-1")
                   .has_value());
  EXPECT_FALSE(
      MediaFileSystemBackend::ParseMountName("media_galleries-Default--1")
          .has_value());

  // Missing, zero (kInvalidMediaGalleryPrefId), or non-numeric pref_id.
  EXPECT_FALSE(MediaFileSystemBackend::ParseMountName(
                   "media_galleries-Default-abcdefghijklmnopabcdefghijklmnop-")
                   .has_value());
  EXPECT_FALSE(MediaFileSystemBackend::ParseMountName(
                   "media_galleries-Default-abcdefghijklmnopabcdefghijklmnop-0")
                   .has_value());
  EXPECT_FALSE(
      MediaFileSystemBackend::ParseMountName(
          "media_galleries-Default-abcdefghijklmnopabcdefghijklmnop-abc")
          .has_value());
  EXPECT_FALSE(
      MediaFileSystemBackend::ParseMountName(
          "media_galleries-Default-abcdefghijklmnopabcdefghijklmnop-12abc")
          .has_value());

  // Mount prefix constructed with kInvalidMediaGalleryPrefId has no pref_id.
  const std::string incomplete_mount =
      MediaFileSystemBackend::ConstructMountName(
          base::FilePath(FILE_PATH_LITERAL("/profiles/Default")), kExtensionIdA,
          kInvalidMediaGalleryPrefId);
  EXPECT_FALSE(
      MediaFileSystemBackend::ParseMountName(incomplete_mount).has_value());
}

TEST(MediaFileSystemBackendTest, EnforcesMountOwnerOnOperationsAndStreams) {
  content::BrowserTaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto backend_ptr =
      std::make_unique<MediaFileSystemBackend>(temp_dir.GetPath());
  MediaFileSystemBackend& backend = *backend_ptr;
  std::vector<std::unique_ptr<storage::FileSystemBackend>> additional_providers;
  additional_providers.push_back(std::move(backend_ptr));
  scoped_refptr<storage::FileSystemContext> context =
      storage::CreateFileSystemContextWithAdditionalProvidersForTesting(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SequencedTaskRunner::GetCurrentDefault(),
          /*quota_manager_proxy=*/nullptr, std::move(additional_providers),
          temp_dir.GetPath());

  const std::string mount_name = MediaFileSystemBackend::ConstructMountName(
      temp_dir.GetPath(), kExtensionIdA, 1);
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      mount_name, storage::kFileSystemTypeLocalMedia,
      storage::FileSystemMountOption(), temp_dir.GetPath()));

  const base::FilePath virtual_path =
      base::FilePath::FromUTF8Unsafe(mount_name).AppendASCII("photo.jpg");

  // 1. Owner extension is allowed.
  const url::Origin owner_origin = url::Origin::Create(
      GURL(std::string("chrome-extension://") + kExtensionIdA));
  const storage::FileSystemURL owner_url =
      mount_points->CreateCrackedFileSystemURL(
          blink::StorageKey::CreateFirstParty(owner_origin),
          storage::kFileSystemTypeExternal, virtual_path);
  ASSERT_TRUE(owner_url.is_valid());

  base::File::Error error = base::File::FILE_OK;
  EXPECT_NE(nullptr, backend.CreateFileSystemOperation(
                         storage::OperationType::kGetMetadata, owner_url,
                         context.get(), &error));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_NE(nullptr, backend.CreateFileStreamReader(owner_url, /*offset=*/0,
                                                    /*max_bytes_to_read=*/10,
                                                    base::Time(), context.get(),
                                                    base::NullCallback()));
  EXPECT_NE(nullptr, backend.CreateFileStreamWriter(owner_url, /*offset=*/0,
                                                    context.get()));

  // 2. Different extension is denied.
  const url::Origin other_ext_origin = url::Origin::Create(
      GURL(std::string("chrome-extension://") + kExtensionIdB));
  const storage::FileSystemURL other_ext_url =
      mount_points->CreateCrackedFileSystemURL(
          blink::StorageKey::CreateFirstParty(other_ext_origin),
          storage::kFileSystemTypeExternal, virtual_path);
  ASSERT_TRUE(other_ext_url.is_valid());

  error = base::File::FILE_OK;
  EXPECT_EQ(nullptr, backend.CreateFileSystemOperation(
                         storage::OperationType::kGetMetadata, other_ext_url,
                         context.get(), &error));
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, error);
  EXPECT_EQ(nullptr, backend.CreateFileStreamReader(other_ext_url, /*offset=*/0,
                                                    /*max_bytes_to_read=*/10,
                                                    base::Time(), context.get(),
                                                    base::NullCallback()));
  EXPECT_EQ(nullptr, backend.CreateFileStreamWriter(other_ext_url, /*offset=*/0,
                                                    context.get()));

  // 3. Non-extension web origin is denied.
  const url::Origin web_origin =
      url::Origin::Create(GURL("https://example.com"));
  const storage::FileSystemURL web_url =
      mount_points->CreateCrackedFileSystemURL(
          blink::StorageKey::CreateFirstParty(web_origin),
          storage::kFileSystemTypeExternal, virtual_path);
  ASSERT_TRUE(web_url.is_valid());

  error = base::File::FILE_OK;
  EXPECT_EQ(nullptr, backend.CreateFileSystemOperation(
                         storage::OperationType::kGetMetadata, web_url,
                         context.get(), &error));
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, error);

  mount_points->RevokeFileSystem(mount_name);
}

}  // namespace
