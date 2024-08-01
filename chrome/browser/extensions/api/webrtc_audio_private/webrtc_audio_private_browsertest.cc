// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

using base::JSONWriter;
using content::RenderProcessHost;
using content::WebContents;
using media::AudioDeviceDescriptions;

namespace extensions {

using api_test_utils::RunFunctionAndReturnSingleResult;

namespace {

// Synchronously (from the calling thread's point of view) runs the
// given enumeration function on the device thread. On return,
// |device_descriptions| has been filled with the device descriptions
// resulting from that call.
void GetAudioDeviceDescriptions(bool for_input,
                                AudioDeviceDescriptions* device_descriptions) {
  base::test::TestFuture<AudioDeviceDescriptions>
      audio_device_descriptions_future;
  std::unique_ptr<media::AudioSystem> audio_system =
      content::CreateAudioSystemForAudioService();
  audio_system->GetDeviceDescriptions(
      for_input, audio_device_descriptions_future.GetCallback());
  *device_descriptions = audio_device_descriptions_future.Take();
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
      if (audio_playing) {
        break;
      }
      base::PlatformThread::Sleep(base::Milliseconds(100));
    }
    if (!audio_playing) {
      FAIL() << "Audio did not start playing within ~5 seconds.";
    }
  }
};

class WebrtcAudioPrivateTest : public AudioWaitingExtensionTest {
 public:
  void SetUpOnMainThread() override {
    AudioWaitingExtensionTest::SetUpOnMainThread();
    // Needs to happen after chrome's schemes are added.
    source_url_ = GURL("chrome-extension://fakeid012345678/fakepage.html");
  }

 protected:
  void AppendTabIdToRequestInfo(base::Value::List* params, int tab_id) {
    base::Value::Dict request_info;
    request_info.Set("tabId", tab_id);
    params->Append(base::Value(std::move(request_info)));
  }

  std::optional<base::Value> InvokeGetSinks() {
    scoped_refptr<WebrtcAudioPrivateGetSinksFunction> function =
        new WebrtcAudioPrivateGetSinksFunction();
    function->set_source_url(source_url_);

    return RunFunctionAndReturnSingleResult(function.get(), "[]", profile());
  }

  std::string GetMediaDeviceIDSalt(const url::Origin& origin) {
    media_device_salt::MediaDeviceSaltService* salt_service =
        MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
            profile());
    base::test::TestFuture<const std::string&> future;
    salt_service->GetSalt(blink::StorageKey::CreateFirstParty(origin),
                          future.GetCallback());
    return future.Get();
  }

  GURL source_url_;
};

#if !BUILDFLAG(IS_MAC)
// http://crbug.com/334579
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetSinks) {
  AudioDeviceDescriptions devices;
  GetAudioDeviceDescriptions(false, &devices);

  std::optional<base::Value> result = InvokeGetSinks();
  const base::Value::List& sink_list = result->GetList();

  std::string result_string;
  JSONWriter::Write(*result, &result_string);
  VLOG(2) << result_string;

  EXPECT_EQ(devices.size(), sink_list.size());

  // Iterate through both lists in lockstep and compare. The order
  // should be identical.
  size_t ix = 0;
  AudioDeviceDescriptions::const_iterator it = devices.begin();
  for (; ix < sink_list.size() && it != devices.end(); ++ix, ++it) {
    const base::Value& value = sink_list[ix];
    EXPECT_TRUE(value.is_dict());
    const base::Value::Dict& dict = value.GetDict();
    const std::string* sink_id = dict.FindString("sinkId");
    EXPECT_TRUE(sink_id);

    url::Origin origin = url::Origin::Create(source_url_);
    std::string expected_id =
        media::AudioDeviceDescription::IsDefaultDevice(it->unique_id)
            ? media::AudioDeviceDescription::kDefaultDeviceId
            : content::GetHMACForMediaDeviceID(GetMediaDeviceIDSalt(origin),
                                               origin, it->unique_id);

    EXPECT_EQ(expected_id, *sink_id);
    const std::string* sink_label = dict.FindString("sinkLabel");
    EXPECT_TRUE(sink_label);
    EXPECT_EQ(it->device_name, *sink_label);

    // TODO(joi): Verify the contents of these once we start actually
    // filling them in.
    EXPECT_TRUE(dict.Find("isDefault"));
    EXPECT_TRUE(dict.Find("isReady"));
    EXPECT_TRUE(dict.Find("sampleRate"));
  }
}
#endif  // BUILDFLAG(IS_MAC)

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
    GURL gurl("http://www.google.com/");
    url::Origin origin = url::Origin::Create(gurl);
    std::string source_id_in_origin = content::GetHMACForMediaDeviceID(
        GetMediaDeviceIDSalt(origin), origin, raw_device_id);

    base::Value::List parameters;
    parameters.Append(gurl.spec());
    parameters.Append(source_id_in_origin);
    std::string parameter_string;
    JSONWriter::Write(parameters, &parameter_string);

    std::optional<base::Value> result = RunFunctionAndReturnSingleResult(
        function.get(), parameter_string, profile());
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
  std::string result =
      ExecuteScriptInBackgroundPageDeprecated(extension->id(), "reportIfGot()");
  EXPECT_EQ("true", result);
}

class HangoutServicesBrowserTest : public AudioWaitingExtensionTest {
 public:
  HangoutServicesBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

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
    // This is necessary to use https with arbitrary hostnames.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    https_server().AddDefaultHandlers(GetChromeTestDataDir());
    host_resolver()->AddRule("*", "127.0.0.1");
    AudioWaitingExtensionTest::SetUpOnMainThread();
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
};

#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
IN_PROC_BROWSER_TEST_F(HangoutServicesBrowserTest,
                       RunComponentExtensionTest) {
  constexpr char kLogUploadUrlPath[] = "/upload_webrtc_log";

  // Set up handling of the log upload request.
  https_server().RegisterRequestHandler(base::BindLambdaForTesting(
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
  ASSERT_TRUE(https_server().Start());

  // This runs the end-to-end JavaScript test for the Hangout Services
  // component extension, which uses the webrtcAudioPrivate API among
  // others.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server().GetURL("any-subdomain.google.com",
                            "/extensions/hangout_services_test.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitUntilAudioIsPlaying(tab);

  // Use a test server URL for uploading.
  g_browser_process->webrtc_log_uploader()->SetUploadUrlForTesting(
      https_server().GetURL("any-subdomain.google.com", kLogUploadUrlPath));

  ASSERT_TRUE(content::ExecJs(tab, "browsertestRunAllTests();"));

  content::TitleWatcher title_watcher(tab, u"success");
  title_watcher.AlsoWaitForTitle(u"failure");
  std::u16string result = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(u"success", result);
}
#endif  // BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)

}  // namespace extensions
