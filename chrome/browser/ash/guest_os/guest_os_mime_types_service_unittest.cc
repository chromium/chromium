// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using vm_tools::apps::MimeTypes;

namespace guest_os {

namespace {

constexpr char kTestVmName[] = "test_vm";
constexpr char kTestContainerName[] = "test_container";

}  // namespace

class GuestOsMimeTypesServiceTest : public testing::Test {
 public:
  GuestOsMimeTypesServiceTest()
      : crostini_test_helper_(&profile_),
        service_(std::make_unique<GuestOsMimeTypesService>(&profile_)) {}
  GuestOsMimeTypesServiceTest(const GuestOsMimeTypesServiceTest&) = delete;
  GuestOsMimeTypesServiceTest& operator=(const GuestOsMimeTypesServiceTest&) =
      delete;

 protected:
  GuestOsMimeTypesService* service() { return service_.get(); }

  Profile* profile() { return &profile_; }

  MimeTypes CreateMimeTypesProto(
      const std::vector<std::string>& file_extensions,
      const std::vector<std::string>& mime_types,
      const std::string& vm_name,
      const std::string& container_name) {
    CHECK_EQ(file_extensions.size(), mime_types.size());
    MimeTypes mime_types_list;
    mime_types_list.set_vm_name(vm_name);
    mime_types_list.set_container_name(container_name);
    for (size_t i = 0; i < file_extensions.size(); ++i) {
      (*mime_types_list.mutable_mime_type_mappings())[file_extensions[i]] =
          mime_types[i];
    }
    return mime_types_list;
  }

  std::string GetMimeType(const std::string& filename) {
    return service_->GetMimeType(base::FilePath(filename), kTestVmName,
                                 kTestContainerName);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  crostini::CrostiniTestHelper crostini_test_helper_;

  std::unique_ptr<GuestOsMimeTypesService> service_;
};

TEST_F(GuestOsMimeTypesServiceTest, SetAndGetMimeTypes) {
  std::vector<std::string> file_extensions = {
      "foo", "bar", "gz", "xz", "tar.gz", "c", "C", "z", "🦈x"};
  std::vector<std::string> mime_types = {"x/foo", "x/bar",    "x/gz",
                                         "x/xz",  "x/tar.gz", "x/c",
                                         "x/C",   "x/z",      "x/shark"};

  // Mime types not registered yet.
  EXPECT_EQ("", GetMimeType("test.foo"));

  service()->UpdateMimeTypes(CreateMimeTypesProto(
      file_extensions, mime_types, kTestVmName, kTestContainerName));

  EXPECT_EQ("x/foo", GetMimeType("test.foo"));
  EXPECT_EQ("x/bar", GetMimeType("test.bar"));
  // Use double extension if possible.
  EXPECT_EQ("x/tar.gz", GetMimeType("test.tar.gz"));
  // Fall back to final extension.
  EXPECT_EQ("x/xz", GetMimeType("test.tar.xz"));
  // Case insensitive match on extension.
  EXPECT_EQ("x/c", GetMimeType("test.c"));
  EXPECT_EQ("x/C", GetMimeType("test.C"));
  EXPECT_EQ("x/z", GetMimeType("test.z"));
  EXPECT_EQ("x/z", GetMimeType("test.Z"));
  EXPECT_EQ("x/tar.gz", GetMimeType("test.tar.GZ"));
  // Support unicode.
  EXPECT_EQ("x/shark", GetMimeType("test.🦈X"));
}

// Test that UpdateMimeTypes doesn't clobber MIME types from different VMs or
// containers.
TEST_F(GuestOsMimeTypesServiceTest, MultipleContainers) {
  service()->UpdateMimeTypes(
      CreateMimeTypesProto({"foo"}, {"foo/mime"}, "vm 1", "container 1"));
  service()->UpdateMimeTypes(
      CreateMimeTypesProto({"bar"}, {"bar/mime"}, "vm 1", "container 2"));
  service()->UpdateMimeTypes(
      CreateMimeTypesProto({"foobar"}, {"foobar/mime"}, "vm 2", "container 1"));

  EXPECT_EQ("foo/mime", service()->GetMimeType(base::FilePath("test.foo"),
                                               "vm 1", "container 1"));
  EXPECT_EQ("bar/mime", service()->GetMimeType(base::FilePath("test.bar"),
                                               "vm 1", "container 2"));
  EXPECT_EQ("foobar/mime", service()->GetMimeType(base::FilePath("test.foobar"),
                                                  "vm 2", "container 1"));

  EXPECT_EQ("", service()->GetMimeType(base::FilePath("test.bar"), "vm 1",
                                       "container 1"));
  EXPECT_EQ("", service()->GetMimeType(base::FilePath("test.foobar"), "vm 1",
                                       "container 2"));
  EXPECT_EQ("", service()->GetMimeType(base::FilePath("test.foo"), "vm 2",
                                       "container 1"));

  // Clobber bar with bar2 and ensure the old association is gone and new one is
  // there.
  service()->UpdateMimeTypes(
      CreateMimeTypesProto({"bar2"}, {"bar2/mime"}, "vm 1", "container 2"));
  EXPECT_EQ("bar2/mime", service()->GetMimeType(base::FilePath("test.bar2"),
                                                "vm 1", "container 2"));
  EXPECT_EQ("", service()->GetMimeType(base::FilePath("test.bar"), "vm 1",
                                       "container 2"));
}

// Test that ClearMimeTypes works, and only removes apps from the
// specified VM.
TEST_F(GuestOsMimeTypesServiceTest, ClearMimeTypes) {
  service()->UpdateMimeTypes(
      CreateMimeTypesProto({"foo"}, {"foo/mime"}, "vm 1", "container 1"));
  service()->UpdateMimeTypes(
      CreateMimeTypesProto({"bar"}, {"bar/mime"}, "vm 1", "container 2"));
  service()->UpdateMimeTypes(
      CreateMimeTypesProto({"foobar"}, {"foobar/mime"}, "vm 2", "container 1"));

  EXPECT_EQ("foo/mime", service()->GetMimeType(base::FilePath("test.foo"),
                                               "vm 1", "container 1"));
  EXPECT_EQ("bar/mime", service()->GetMimeType(base::FilePath("test.bar"),
                                               "vm 1", "container 2"));
  EXPECT_EQ("foobar/mime", service()->GetMimeType(base::FilePath("test.foobar"),
                                                  "vm 2", "container 1"));

  service()->ClearMimeTypes("vm 2", "");

  EXPECT_EQ("foo/mime", service()->GetMimeType(base::FilePath("test.foo"),
                                               "vm 1", "container 1"));
  EXPECT_EQ("bar/mime", service()->GetMimeType(base::FilePath("test.bar"),
                                               "vm 1", "container 2"));
  EXPECT_EQ("", service()->GetMimeType(base::FilePath("test.foobar"), "vm 2",
                                       "container 1"));
}

TEST_F(GuestOsMimeTypesServiceTest, Migrate) {
  base::Value old = *base::JSONReader::Read(R"({
    "termina/penguin/txt": { "mime_type": "text/plain" },
    "termina/penguin/jpg": { "mime_type": "image/jpg" },
    "termina/penguin/tar.gz": { "mime_type": "application/tar+gzip" },
    "borealis/penguin/txt": { "mime_type": "text/plain" },
    "termina": {
      "penguin": {
        "already": "x/already-migrated"
      }
    },
    "delete/me": { "mime_type": "text/plain" },
    "delete/me/as/well": { "mime_type": "text/plain" }
  })");

  base::Value expected = *base::JSONReader::Read(R"({
    "termina": {
      "penguin": {
        "already": "x/already-migrated",
        "txt": "text/plain",
        "jpg": "image/jpg",
        "tar.gz": "application/tar+gzip"
      }
    },
    "borealis": {
      "penguin": {
        "txt": "text/plain"
      }
    }
  })");

  PrefService* prefs = profile()->GetPrefs();
  prefs->Set("crostini.mime_types", std::move(old));
  GuestOsMimeTypesService::MigrateVerboseMimeTypePrefs(prefs);
  EXPECT_EQ(expected, prefs->Get("crostini.mime_types")->Clone());
}

}  // namespace guest_os
