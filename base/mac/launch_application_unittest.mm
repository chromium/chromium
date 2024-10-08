// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/launch_application.h"

#include <sys/select.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace base::mac {
namespace {

// Reads XML encoded property lists from `fifo_path`, calling `callback` for
// each succesfully parsed dictionary. Loops indefinitely until the string
// "<!FINISHED>" is read from `fifo_path`.
void ReadLaunchEventsFromFifo(
    const FilePath& fifo_path,
    RepeatingCallback<void(NSDictionary* event)> callback) {
  File f(fifo_path, File::FLAG_OPEN | File::FLAG_READ);
  std::string data;
  while (true) {
    char buf[4096];
    std::optional<size_t> read_count =
        f.ReadAtCurrentPosNoBestEffort(base::as_writable_byte_span(buf));
    ASSERT_TRUE(read_count.has_value());
    if (read_count.value()) {
      data += std::string_view(buf, read_count.value());
      // Assume that at any point the beginning of the data buffer is the start
      // of a plist. Search for the first end, and parse that substring.
      size_t end_of_plist;
      while ((end_of_plist = data.find("</plist>")) != std::string::npos) {
        std::string plist = data.substr(0, end_of_plist + 8);
        data = data.substr(plist.length());
        NSDictionary* event = apple::ObjCCastStrict<NSDictionary>(
            SysUTF8ToNSString(TrimWhitespaceASCII(plist, TRIM_ALL))
                .propertyList);
        callback.Run(event);
      }
      // No more plists found, check if the termination marker was send.
      if (data.find("<!FINISHED>") != std::string::npos) {
        break;
      }
    } else {
      // No data was read, wait for the file descriptor to become readable
      // again.
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(f.GetPlatformFile(), &fds);
      select(FD_SETSIZE, &fds, nullptr, nullptr, nullptr);
    }
  }
}

// This test harness creates an app bundle with a random bundle identifier to
// avoid conflicts with concurrently running other tests. The binary in this app
// bundle writes various events to a named pipe, allowing tests here to verify
// that correct events were received by the app.
class LaunchApplicationTest : public testing::Test {
 public:
  void SetUp() override {
    helper_bundle_id_ =
        SysUTF8ToNSString("org.chromium.LaunchApplicationTestHelper." +
                          Uuid::GenerateRandomV4().AsLowercaseString());

    FilePath data_root;
    ASSERT_TRUE(PathService::Get(DIR_OUT_TEST_DATA_ROOT, &data_root));
    const FilePath helper_app_executable =
        data_root.AppendASCII("launch_application_test_helper");

    // Put helper app inside home dir, as the default temp location gets special
    // treatment by launch services, effecting the behavior of some of these
    // tests.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(base::GetHomeDir()));

    helper_app_bundle_path_ =
        temp_dir_.GetPath().AppendASCII("launch_application_test_helper.app");

    const base::FilePath destination_contents_path =
        helper_app_bundle_path_.AppendASCII("Contents");
    const base::FilePath destination_executable_path =
        destination_contents_path.AppendASCII("MacOS");

    // First create the .app bundle directory structure.
    // Use NSFileManager so that the permissions can be set appropriately. The
    // base::CreateDirectory() routine forces mode 0700.
    NSError* error = nil;
    ASSERT_TRUE([NSFileManager.defaultManager
               createDirectoryAtURL:base::apple::FilePathToNSURL(
                                        destination_executable_path)
        withIntermediateDirectories:YES
                         attributes:@{
                           NSFilePosixPermissions : @(0755)
                         }
                              error:&error])
        << SysNSStringToUTF8(error.description);

    // Copy the executable file.
    helper_app_executable_path_ =
        destination_executable_path.Append(helper_app_executable.BaseName());
    ASSERT_TRUE(
        base::CopyFile(helper_app_executable, helper_app_executable_path_));

    // Write the PkgInfo file.
    constexpr char kPkgInfoData[] = "APPL????";
    ASSERT_TRUE(base::WriteFile(
        destination_contents_path.AppendASCII("PkgInfo"), kPkgInfoData));

#if defined(ADDRESS_SANITIZER)
    const base::FilePath asan_library_path =
        data_root.AppendASCII("libclang_rt.asan_osx_dynamic.dylib");
    ASSERT_TRUE(base::CopyFile(
        asan_library_path,
        destination_executable_path.Append(asan_library_path.BaseName())));
#endif

#if defined(UNDEFINED_SANITIZER)
    const base::FilePath ubsan_library_path =
        data_root.AppendASCII("libclang_rt.ubsan_osx_dynamic.dylib");
    ASSERT_TRUE(base::CopyFile(
        ubsan_library_path,
        destination_executable_path.Append(ubsan_library_path.BaseName())));
#endif

    // Generate the Plist file
    NSDictionary* plist = @{
      @"CFBundleExecutable" :
          apple::FilePathToNSString(helper_app_executable.BaseName()),
      @"CFBundleIdentifier" : helper_bundle_id_,
    };
    ASSERT_TRUE([plist
        writeToURL:apple::FilePathToNSURL(
                       destination_contents_path.AppendASCII("Info.plist"))
             error:nil]);

    // Register the app with LaunchServices.
    LSRegisterURL(base::apple::FilePathToCFURL(helper_app_bundle_path_).get(),
                  true);

    // Ensure app was registered with LaunchServices. Sometimes it takes a
    // little bit of time for this to happen, and some tests might fail if the
    // app wasn't registered yet.
    while (true) {
      NSArray<NSURL*>* apps = nil;
      if (@available(macOS 12.0, *)) {
        apps = [[NSWorkspace sharedWorkspace]
            URLsForApplicationsWithBundleIdentifier:helper_bundle_id_];
      } else {
        apps =
            apple::CFToNSOwnershipCast(LSCopyApplicationURLsForBundleIdentifier(
                apple::NSToCFPtrCast(helper_bundle_id_), /*outError=*/nullptr));
      }
      if (apps.count > 0) {
        break;
      }
      PlatformThread::Sleep(Milliseconds(50));
    }

    // Setup fifo to receive logs from the helper app.
    helper_app_fifo_path_ =
        temp_dir_.GetPath().AppendASCII("launch_application_test_helper.fifo");
    ASSERT_EQ(0, mkfifo(helper_app_fifo_path_.value().c_str(),
                        S_IWUSR | S_IRUSR | S_IWGRP | S_IWGRP));

    // Create array to store received events in, and start listening for events.
    launch_events_ = [[NSMutableArray alloc] init];
    base::ThreadPool::PostTask(
        FROM_HERE, {MayBlock()},
        base::BindOnce(
            &ReadLaunchEventsFromFifo, helper_app_fifo_path_,
            BindPostTaskToCurrentDefault(BindRepeating(
                &LaunchApplicationTest::OnLaunchEvent, Unretained(this)))));
  }

  void TearDown() override {
    if (temp_dir_.IsValid()) {
      // Make sure fifo reading task stops reading/waiting.
      WriteFile(helper_app_fifo_path_, "<!FINISHED>");

      // Make sure all apps that were launched by this test are terminated.
      NSArray<NSRunningApplication*>* apps =
          NSWorkspace.sharedWorkspace.runningApplications;
      for (NSRunningApplication* app in apps) {
        if (temp_dir_.GetPath().IsParent(
                apple::NSURLToFilePath(app.bundleURL)) ||
            [app.bundleIdentifier isEqualToString:helper_bundle_id_]) {
          [app forceTerminate];
        }
      }

      // And make sure the temp dir was successfully deleted.
      EXPECT_TRUE(temp_dir_.Delete());
    }
  }

  // Calls `LaunchApplication` with the given parameters, expecting the launch
  // to succeed. Returns the `NSRunningApplication*` the callback passed to
  // `LaunchApplication` was called with.
  NSRunningApplication* LaunchApplicationSyncExpectSuccess(
      const FilePath& app_bundle_path,
      const CommandLineArgs& command_line_args,
      const std::vector<std::string>& url_specs,
      LaunchApplicationOptions options) {
    test::TestFuture<NSRunningApplication*, NSError*> result;
    LaunchApplication(app_bundle_path, command_line_args, url_specs, options,
                      result.GetCallback());
    EXPECT_FALSE(result.Get<1>());
    EXPECT_TRUE(result.Get<0>());
    return result.Get<0>();
  }

  // Similar to the above method, except that this version expects the launch to
  // fail, returning the error.
  NSError* LaunchApplicationSyncExpectError(
      const FilePath& app_bundle_path,
      const CommandLineArgs& command_line_args,
      const std::vector<std::string>& url_specs,
      LaunchApplicationOptions options) {
    test::TestFuture<NSRunningApplication*, NSError*> result;
    LaunchApplication(app_bundle_path, command_line_args, url_specs, options,
                      result.GetCallback());
    EXPECT_FALSE(result.Get<0>());
    EXPECT_TRUE(result.Get<1>());
    return result.Get<1>();
  }

  // Waits for the total number of received launch events to reach at least
  // `expected_count`.
  void WaitForLaunchEvents(unsigned expected_count) {
    if (LaunchEventCount() >= expected_count) {
      return;
    }
    base::RunLoop loop;
    launch_event_callback_ = BindLambdaForTesting([&] {
      if (LaunchEventCount() >= expected_count) {
        launch_event_callback_ = NullCallback();
        loop.Quit();
      }
    });
    loop.Run();
  }

  unsigned LaunchEventCount() { return launch_events_.count; }
  NSString* LaunchEventName(unsigned i) {
    if (i >= launch_events_.count) {
      return nil;
    }
    return apple::ObjCCastStrict<NSString>(launch_events_[i][@"name"]);
  }
  NSDictionary* LaunchEventData(unsigned i) {
    if (i >= launch_events_.count) {
      return nil;
    }
    return apple::ObjCCastStrict<NSDictionary>(launch_events_[i][@"data"]);
  }

 protected:
  ScopedTempDir temp_dir_;

  NSString* helper_bundle_id_;
  FilePath helper_app_bundle_path_;
  FilePath helper_app_executable_path_;
  FilePath helper_app_fifo_path_;

  NSMutableArray<NSDictionary*>* launch_events_;
  RepeatingClosure launch_event_callback_;

  test::TaskEnvironment task_environment_{
      test::TaskEnvironment::MainThreadType::UI};

 private:
  void OnLaunchEvent(NSDictionary* event) {
    NSLog(@"Event: %@", event);
    [launch_events_ addObject:event];
    if (launch_event_callback_) {
      launch_event_callback_.Run();
    }
  }
};

TEST_F(LaunchApplicationTest, Basic) {
  std::vector<std::string> command_line_args;
  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {}, {});
  ASSERT_TRUE(app);
  EXPECT_NSEQ(app.bundleIdentifier, helper_bundle_id_);
  EXPECT_EQ(apple::NSURLToFilePath(app.bundleURL), helper_app_bundle_path_);

  WaitForLaunchEvents(1);
  EXPECT_NSEQ(LaunchEventName(0), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(0)[@"activationPolicy"],
              @(NSApplicationActivationPolicyRegular));
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyRegular);
  EXPECT_NSEQ(LaunchEventData(0)[@"commandLine"],
              (@[ apple::FilePathToNSString(helper_app_executable_path_) ]));
  EXPECT_NSEQ(LaunchEventData(0)[@"processIdentifier"],
              @(app.processIdentifier));
}

TEST_F(LaunchApplicationTest, BundleDoesntExist) {
  std::vector<std::string> command_line_args;
  NSError* err = LaunchApplicationSyncExpectError(
      temp_dir_.GetPath().AppendASCII("notexists.app"), command_line_args, {},
      {});
  ASSERT_TRUE(err);
  err = LaunchApplicationSyncExpectError(
      temp_dir_.GetPath().AppendASCII("notexists.app"), command_line_args, {},
      {.hidden_in_background = true});
  ASSERT_TRUE(err);
}

TEST_F(LaunchApplicationTest, BundleCorrupt) {
  base::DeleteFile(helper_app_executable_path_);
  std::vector<std::string> command_line_args;
  NSError* err = LaunchApplicationSyncExpectError(helper_app_bundle_path_,
                                                  command_line_args, {}, {});
  ASSERT_TRUE(err);
  err = LaunchApplicationSyncExpectError(helper_app_bundle_path_,
                                         command_line_args, {},
                                         {.hidden_in_background = true});
  ASSERT_TRUE(err);
}

TEST_F(LaunchApplicationTest, CommandLineArgs_StringVector) {
  std::vector<std::string> command_line_args = {"--foo", "bar", "-v"};
  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {}, {});
  ASSERT_TRUE(app);

  WaitForLaunchEvents(1);
  EXPECT_NSEQ(LaunchEventName(0), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(0)[@"commandLine"], (@[
                apple::FilePathToNSString(helper_app_executable_path_),
                @"--foo", @"bar", @"-v"
              ]));
}

TEST_F(LaunchApplicationTest, CommandLineArgs_BaseCommandLine) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("foo", "bar");
  command_line.AppendSwitch("v");
  command_line.AppendSwitchPath("path", FilePath("/tmp"));

  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line, {}, {});
  ASSERT_TRUE(app);

  WaitForLaunchEvents(1);
  EXPECT_NSEQ(LaunchEventName(0), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(0)[@"commandLine"], (@[
                apple::FilePathToNSString(helper_app_executable_path_),
                @"--foo=bar", @"--v", @"--path=/tmp"
              ]));
}

TEST_F(LaunchApplicationTest, UrlSpecs) {
  std::vector<std::string> command_line_args;
  std::vector<std::string> urls = {"https://example.com",
                                   "x-chrome-launch://1"};
  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, urls, {});
  ASSERT_TRUE(app);
  WaitForLaunchEvents(3);

  EXPECT_NSEQ(LaunchEventName(0), @"openURLs");
  EXPECT_NSEQ(LaunchEventName(1), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventName(2), @"openURLs");

  if (MacOSMajorVersion() == 11) {
    // macOS 11 (and only macOS 11) appears to sometimes trigger the openURLs
    // calls in reverse order.
    std::vector<std::string> received_urls;
    for (NSString* url in apple::ObjCCastStrict<NSArray>(
             LaunchEventData(0)[@"urls"])) {
      received_urls.push_back(SysNSStringToUTF8(url));
    }
    EXPECT_EQ(received_urls.size(), 1u);
    for (NSString* url in apple::ObjCCastStrict<NSArray>(
             LaunchEventData(2)[@"urls"])) {
      received_urls.push_back(SysNSStringToUTF8(url));
    }
    EXPECT_THAT(received_urls, testing::UnorderedElementsAreArray(urls));
  } else {
    EXPECT_NSEQ(LaunchEventData(0)[@"urls"], @[ @"https://example.com" ]);
    EXPECT_NSEQ(LaunchEventData(2)[@"urls"], @[ @"x-chrome-launch://1" ]);
  }
}

TEST_F(LaunchApplicationTest, CreateNewInstance) {
  std::vector<std::string> command_line_args;
  NSRunningApplication* app1 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {},
      {.create_new_instance = false});
  WaitForLaunchEvents(1);
  EXPECT_NSEQ(LaunchEventName(0), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(0)[@"processIdentifier"],
              @(app1.processIdentifier));

  NSRunningApplication* app2 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {"x-chrome-launch://0"},
      {.create_new_instance = false});
  EXPECT_NSEQ(app1, app2);
  EXPECT_EQ(app1.processIdentifier, app2.processIdentifier);
  WaitForLaunchEvents(2);
  EXPECT_NSEQ(LaunchEventName(1), @"openURLs");
  EXPECT_NSEQ(LaunchEventData(1)[@"processIdentifier"],
              @(app2.processIdentifier));

  NSRunningApplication* app3 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {"x-chrome-launch://1"},
      {.create_new_instance = true});
  EXPECT_NSNE(app1, app3);
  EXPECT_NE(app1.processIdentifier, app3.processIdentifier);
  WaitForLaunchEvents(4);
  EXPECT_NSEQ(LaunchEventName(2), @"openURLs");
  EXPECT_NSEQ(LaunchEventName(3), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(3)[@"processIdentifier"],
              @(app3.processIdentifier));
}

TEST_F(LaunchApplicationTest, HiddenInBackground) {
  std::vector<std::string> command_line_args = {"--test", "--foo"};
  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {},
      {.hidden_in_background = true});
  ASSERT_TRUE(app);
  EXPECT_NSEQ(app.bundleIdentifier, helper_bundle_id_);
  EXPECT_EQ(helper_app_bundle_path_, apple::NSURLToFilePath(app.bundleURL));

  WaitForLaunchEvents(1);
  EXPECT_NSEQ(LaunchEventName(0), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(0)[@"activationPolicy"],
              @(NSApplicationActivationPolicyProhibited));
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyProhibited);
  EXPECT_NSEQ(LaunchEventData(0)[@"commandLine"], (@[
                apple::FilePathToNSString(helper_app_executable_path_),
                @"--test", @"--foo"
              ]));
  EXPECT_NSEQ(LaunchEventData(0)[@"processIdentifier"],
              @(app.processIdentifier));

  NSRunningApplication* app2 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {},
      {.create_new_instance = false, .hidden_in_background = true});
  EXPECT_NSEQ(app, app2);
  EXPECT_EQ(app.processIdentifier, app2.processIdentifier);
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyProhibited);
  EXPECT_EQ(app2.activationPolicy, NSApplicationActivationPolicyProhibited);
  // Launching without opening anything should not trigger any launch events.

  // Opening a URL in a new instance, should leave both instances in the
  // background.
  NSRunningApplication* app3 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {"x-chrome-launch://2"},
      {.create_new_instance = true, .hidden_in_background = true});
  EXPECT_NSNE(app, app3);
  EXPECT_NE(app.processIdentifier, app3.processIdentifier);
  WaitForLaunchEvents(3);
  EXPECT_NSEQ(LaunchEventName(1), @"openURLs");
  EXPECT_NSEQ(LaunchEventName(2), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(2)[@"processIdentifier"],
              @(app3.processIdentifier));
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyProhibited);
  EXPECT_EQ(app2.activationPolicy, NSApplicationActivationPolicyProhibited);
  EXPECT_EQ(app3.activationPolicy, NSApplicationActivationPolicyProhibited);
}

TEST_F(LaunchApplicationTest,
       HiddenInBackground_OpenUrlChangesActivationPolicy) {
  std::vector<std::string> command_line_args = {"--test", "--foo"};
  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {},
      {.hidden_in_background = true});
  ASSERT_TRUE(app);
  EXPECT_NSEQ(app.bundleIdentifier, helper_bundle_id_);
  EXPECT_EQ(helper_app_bundle_path_, apple::NSURLToFilePath(app.bundleURL));

  WaitForLaunchEvents(1);
  EXPECT_NSEQ(LaunchEventName(0), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(0)[@"activationPolicy"],
              @(NSApplicationActivationPolicyProhibited));
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyProhibited);
  EXPECT_NSEQ(LaunchEventData(0)[@"commandLine"], (@[
                apple::FilePathToNSString(helper_app_executable_path_),
                @"--test", @"--foo"
              ]));
  EXPECT_NSEQ(LaunchEventData(0)[@"processIdentifier"],
              @(app.processIdentifier));

  NSRunningApplication* app2 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {"chrome://app-launch/0"},
      {.create_new_instance = false, .hidden_in_background = true});
  EXPECT_NSEQ(app, app2);
  EXPECT_EQ(app.processIdentifier, app2.processIdentifier);
  // Unexpected to me, but opening a URL seems to always change the activation
  // policy.
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyRegular);
  EXPECT_EQ(app2.activationPolicy, NSApplicationActivationPolicyRegular);
  WaitForLaunchEvents(3);
  EXPECT_THAT(
      std::vector<std::string>({SysNSStringToUTF8(LaunchEventName(1)),
                                SysNSStringToUTF8(LaunchEventName(2))}),
      testing::UnorderedElementsAre("activationPolicyChanged", "openURLs"));
}

TEST_F(LaunchApplicationTest, HiddenInBackground_TransitionToForeground) {
  std::vector<std::string> command_line_args;
  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {"x-chrome-launch://1"},
      {.hidden_in_background = true});
  ASSERT_TRUE(app);

  WaitForLaunchEvents(2);
  EXPECT_NSEQ(LaunchEventName(0), @"openURLs");
  EXPECT_NSEQ(LaunchEventName(1), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(1)[@"activationPolicy"],
              @(NSApplicationActivationPolicyProhibited));
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyProhibited);
  EXPECT_NSEQ(LaunchEventData(1)[@"processIdentifier"],
              @(app.processIdentifier));

  // Second launch with hidden_in_background set to false should cause the first
  // app to switch activation policy.
  NSRunningApplication* app2 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {},
      {.hidden_in_background = false});
  EXPECT_NSEQ(app, app2);
  WaitForLaunchEvents(3);
  EXPECT_NSEQ(LaunchEventName(2), @"activationPolicyChanged");
  EXPECT_NSEQ(LaunchEventData(2)[@"activationPolicy"],
              @(NSApplicationActivationPolicyRegular));
  EXPECT_EQ(app2.activationPolicy, NSApplicationActivationPolicyRegular);
}

TEST_F(LaunchApplicationTest, HiddenInBackground_AlreadyInForeground) {
  std::vector<std::string> command_line_args;
  NSRunningApplication* app = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {"x-chrome-launch://1"},
      {.hidden_in_background = false});
  ASSERT_TRUE(app);

  WaitForLaunchEvents(2);
  EXPECT_NSEQ(LaunchEventName(0), @"openURLs");
  EXPECT_NSEQ(LaunchEventName(1), @"applicationDidFinishLaunching");
  EXPECT_NSEQ(LaunchEventData(1)[@"activationPolicy"],
              @(NSApplicationActivationPolicyRegular));
  EXPECT_EQ(app.activationPolicy, NSApplicationActivationPolicyRegular);
  EXPECT_NSEQ(LaunchEventData(1)[@"processIdentifier"],
              @(app.processIdentifier));

  // Second (and third) launch with hidden_in_background set to true should
  // reuse the existing app and keep it visible.
  NSRunningApplication* app2 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {},
      {.hidden_in_background = true});
  EXPECT_NSEQ(app, app2);
  EXPECT_EQ(app2.activationPolicy, NSApplicationActivationPolicyRegular);
  NSRunningApplication* app3 = LaunchApplicationSyncExpectSuccess(
      helper_app_bundle_path_, command_line_args, {"x-chrome-launch://23"},
      {.hidden_in_background = true});
  EXPECT_NSEQ(app, app3);
  WaitForLaunchEvents(3);
  EXPECT_NSEQ(LaunchEventName(2), @"openURLs");
  EXPECT_NSEQ(LaunchEventData(2)[@"processIdentifier"],
              @(app.processIdentifier));
  EXPECT_EQ(app3.activationPolicy, NSApplicationActivationPolicyRegular);
}

}  // namespace
}  // namespace base::mac
