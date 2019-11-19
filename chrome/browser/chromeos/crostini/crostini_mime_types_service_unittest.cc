// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using vm_tools::apps::MimeTypes;

namespace crostini {

class CrostiniMimeTypesServiceTest : public testing::Test {
 public:
  CrostiniMimeTypesServiceTest()
      : crostini_test_helper_(&profile_),
        service_(std::make_unique<CrostiniMimeTypesService>(&profile_)) {}

 protected:
  CrostiniMimeTypesService* service() { return service_.get(); }

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

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  CrostiniTestHelper crostini_test_helper_;

  std::unique_ptr<CrostiniMimeTypesService> service_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniMimeTypesServiceTest);
};

TEST_F(CrostiniMimeTypesServiceTest, SetAndGetMimeTypes) {
  base::FilePath test_path_foo("test.foo");
  base::FilePath test_path_bar("test.bar");
  std::vector<std::string> file_extensions = {"foo", "bar"};
  std::vector<std::string> mime_types = {"test/foo-mime", "bar/x-test"};
  std::string vm_name = "awesomevm";
  std::string container_name = "awesomecontainer";

  EXPECT_EQ("", service()->GetMimeType(test_path_foo, vm_name, container_name));

  service()->UpdateMimeTypes(CreateMimeTypesProto(file_extensions, mime_types,
                                                  vm_name, container_name));
  EXPECT_EQ(mime_types[0],
            service()->GetMimeType(test_path_foo, vm_name, container_name));
  EXPECT_EQ(mime_types[1],
            service()->GetMimeType(test_path_bar, vm_name, container_name));
}

// Test that UpdateMimeTypes doesn't clobber MIME types from different VMs or
// containers.
TEST_F(CrostiniMimeTypesServiceTest, MultipleContainers) {
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
TEST_F(CrostiniMimeTypesServiceTest, ClearMimeTypes) {
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

}  // namespace crostini
