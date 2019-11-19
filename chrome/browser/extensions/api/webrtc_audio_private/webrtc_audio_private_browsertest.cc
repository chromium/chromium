// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/audio/public/cpp/audio_system_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using base::JSONWriter;
using content::RenderProcessHost;
using content::WebContents;
using media::AudioDeviceDescriptions;

namespace extensions {

using extension_function_test_utils::RunFunctionAndReturnError;
using extension_function_test_utils::RunFunctionAndReturnSingleResult;

namespace {

// Synchronously (from the calling thread's point of view) runs the
// given enumeration function on the device thread. On return,
// |device_descriptions| has been filled with the device descriptions
// resulting from that call.
void GetAudioDeviceDescriptions(bool for_input,
                                AudioDeviceDescriptions* device_descriptions) {
  base::RunLoop run_loop;
  std::unique_ptr<media::AudioSystem> audio_system =
      audio::CreateAudioSystem(content::GetSystemConnector()->Clone());
  audio_system->GetDeviceDescriptions(
      for_input,
      base::BindOnce(
          [](base::Closure finished_callback, AudioDeviceDescriptions* result,
             AudioDeviceDescriptions received) {
            *result = std::move(received);
            finished_callback.Run();
          },
          run_loop.QuitClosure(), device_descriptions));
  run_loop.Run();
}

}  // namespace

class AudioWaitingExtensionTest : public ExtensionApiTest {
 protected:
  void WaitUntilAudioIsPlaying(WebContents* tab) {
    // Wait for audio to start playing.
    bool audio_playing = false;
    for (size_t remaining_tries = 50; remaining_tries > 0; --remaining_tries) {
      auto* audible_helper = RecentlyAudibleHelper::FromWebContents(tab);
      audio_playing = audible_helper->WasRecentlyAudible();
      base::RunLoop().RunUntilIdle();
      if (audio_playing)
        break;
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    }
    if (!audio_playing)
      FAIL() << "Audio did not start playing within ~5 seconds.";
  }
};

class WebrtcAudioPrivateTest : public AudioWaitingExtensionTest {
 public:
  WebrtcAudioPrivateTest() {}

  void SetUpOnMainThread() override {
    AudioWaitingExtensionTest::SetUpOnMainThread();
    // Needs to happen after chrome's schemes are added.
    source_url_ = GURL("chrome-extension://fakeid012345678/fakepage.html");
  }

 protected:
  void AppendTabIdToRequestInfo(base::ListValue* params, int tab_id) {
    std::unique_ptr<base::DictionaryValue> request_info(
        new base::DictionaryValue());
    request_info->SetInteger("tabId", tab_id);
    params->Append(std::move(request_info));
  }

  std::unique_ptr<base::Value> InvokeGetSinks(base::ListValue** sink_list) {
    scoped_refptr<WebrtcAudioPrivateGetSinksFunction> function =
        new WebrtcAudioPrivateGetSinksFunction();
    function->set_source_url(source_url_);

    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnSingleResult(function.get(), "[]", browser()));
    result->GetAsList(sink_list);
    return result;
  }

  GURL source_url_;
};

#if !defined(OS_MACOSX)
// http://crbug.com/334579
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetSinks) {
  AudioDeviceDescriptions devices;
  GetAudioDeviceDescriptions(false, &devices);

  base::ListValue* sink_list = NULL;
  std::unique_ptr<base::Value> result = InvokeGetSinks(&sink_list);

  std::string result_string;
  JSONWriter::Write(*result, &result_string);
  VLOG(2) << result_string;

  EXPECT_EQ(devices.size(), sink_list->GetSize());

  // Iterate through both lists in lockstep and compare. The order
  // should be identical.
  size_t ix = 0;
  AudioDeviceDescriptions::const_iterator it = devices.begin();
  for (; ix < sink_list->GetSize() && it != devices.end();
       ++ix, ++it) {
    base::DictionaryValue* dict = NULL;
    sink_list->GetDictionary(ix, &dict);
    std::string sink_id;
    dict->GetString("sinkId", &sink_id);

    std::string expected_id =
        media::AudioDeviceDescription::IsDefaultDevice(it->unique_id)
            ? media::AudioDeviceDescription::kDefaultDeviceId
            : content::GetHMACForMediaDeviceID(
                  profile()->GetMediaDeviceIDSalt(),
                  url::Origin::Create(source_url_.GetOrigin()), it->unique_id);

    EXPECT_EQ(expected_id, sink_id);
    std::string sink_label;
    dict->GetString("sinkLabel", &sink_label);
    EXPECT_EQ(it->device_name, sink_label);

    // TODO(joi): Verify the contents of these once we start actually
    // filling them in.
    EXPECT_TRUE(dict->HasKey("isDefault"));
    EXPECT_TRUE(dict->HasKey("isReady"));
    EXPECT_TRUE(dict->HasKey("sampleRate"));
  }
}
#endif  // OS_MACOSX

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetAssociatedSink) {
  // Get the list of input devices. We can cheat in the unit test and
  // run this on the main thread since nobody else will be running at
  // the same time.
  AudioDeviceDescriptions devices;
  GetAudioDeviceDescriptions(true, &devices);

  // Try to get an associated sink for each source.
  for (const auto& device : devices) {
    scoped_refptr<WebrtcAudioPrivateGetAssociatedSinkFunction> function =
        new WebrtcAudioPrivateGetAssociatedSinkFunction();
    function->set_source_url(source_url_);

    std::string raw_device_id = device.unique_id;
    VLOG(2) << "Trying to find associated sink for device " << raw_device_id;
    GURL origin(GURL("http://www.google.com/").GetOrigin());
    std::string source_id_in_origin = content::GetHMACForMediaDeviceID(
        profile()->GetMediaDeviceIDSalt(), url::Origin::Create(origin),
        raw_device_id);

    base::ListValue parameters;
    parameters.AppendString(origin.spec());
    parameters.AppendString(source_id_in_origin);
    std::string parameter_string;
    JSONWriter::Write(parameters, &parameter_string);

    std::unique_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
        function.get(), parameter_string, browser()));
    std::string result_string;
    JSONWriter::Write(*result, &result_string);
    VLOG(2) << "Results: " << result_string;
  }
}

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, TriggerEvent) {
  WebrtcAudioPrivateEventService* service =
      WebrtcAudioPrivateEventService::GetFactoryInstance()->Get(profile());

  // Just trigger, without any extension listening.
  service->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);

  // Now load our test extension and do it again.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("webrtc_audio_private_event_listener"));
  service->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);

  // Check that the extension got the notification.
  std::string result = ExecuteScriptInBackgroundPage(extension->id(),
                                                     "reportIfGot()");
  EXPECT_EQ("true", result);
}

class HangoutServicesBrowserTest : public AudioWaitingExtensionTest {
 public:
  void SetUp() override {
    // Make sure the Hangout Services component extension gets loaded.
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    AudioWaitingExtensionTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AudioWaitingExtensionTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }
};

#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
IN_PROC_BROWSER_TEST_F(HangoutServicesBrowserTest,
                       RunComponentExtensionTest) {
  constexpr char kLogUploadUrlPath[] = "/upload_webrtc_log";

  // Set up handling of the log upload request.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == kLogUploadUrlPath) {
          std::unique_ptr<net::test_server::BasicHttpResponse> response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content("report_id");
          return std::move(response);
        }

        return nullptr;
      }));

  // This runs the end-to-end JavaScript test for the Hangout Services
  // component extension, which uses the webrtcAudioPrivate API among
  // others.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url(embedded_test_server()->GetURL(
               "/extensions/hangout_services_test.html"));
  // The "externally connectable" extension permission doesn't seem to
  // like when we use 127.0.0.1 as the host, but using localhost works.
  std::string url_spec = url.spec();
  base::ReplaceFirstSubstringAfterOffset(
      &url_spec, 0, "127.0.0.1", "localhost");
  GURL localhost_url(url_spec);
  ui_test_utils::NavigateToURL(browser(), localhost_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitUntilAudioIsPlaying(tab);

  // Use a test server URL for uploading.
  g_browser_process->webrtc_log_uploader()->SetUploadUrlForTesting(
      embedded_test_server()->GetURL(kLogUploadUrlPath));

  ASSERT_TRUE(content::ExecuteScript(tab, "browsertestRunAllTests();"));

  content::TitleWatcher title_watcher(tab, base::ASCIIToUTF16("success"));
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("failure"));
  base::string16 result = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(base::ASCIIToUTF16("success"), result);
}
#endif  // BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)

}  // namespace extensions
