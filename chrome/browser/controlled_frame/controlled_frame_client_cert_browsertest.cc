// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace controlled_frame {

namespace {

// Stub ClientCertStore that returns a list of fake client certificate
// identities.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  explicit ClientCertStoreStub(net::ClientCertIdentityList list)
      : list_(std::move(list)) {}

  ~ClientCertStoreStub() override = default;

  // net::ClientCertStore:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(list_));
    if (GetQuitClosure()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(GetQuitClosure()));
    }
  }

  static void SetQuitClosure(base::OnceClosure quit_closure) {
    GetQuitClosure() = std::move(quit_closure);
  }

 private:
  static base::OnceClosure& GetQuitClosure() {
    static base::NoDestructor<base::OnceClosure> quit_closure;
    return *quit_closure;
  }

  net::ClientCertIdentityList list_;
};

std::unique_ptr<net::ClientCertStore> CreateCertStore() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  net::ClientCertIdentityList cert_identity_list;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::unique_ptr<net::FakeClientCertIdentity> cert_identity_1 =
        net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
            certs_dir, "client_1.pem", "client_1.pk8");
    CHECK(cert_identity_1);
    cert_identity_list.push_back(std::move(cert_identity_1));

    std::unique_ptr<net::FakeClientCertIdentity> cert_identity_2 =
        net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
            certs_dir, "client_2.pem", "client_2.pk8");
    CHECK(cert_identity_2);
    cert_identity_list.push_back(std::move(cert_identity_2));
  }
  return std::make_unique<ClientCertStoreStub>(std::move(cert_identity_list));
}

void SetAutoSelectCertificateForUrl(Profile* profile, const GURL& url) {
  base::ListValue filters;
  filters.Append(base::DictValue());
  base::DictValue setting;
  setting.Set("filters", std::move(filters));
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value(std::move(setting)));
}

web_modal::WebContentsModalDialogManager* GetModalDialogManager(
    content::RenderFrameHost* app_frame) {
  return web_modal::WebContentsModalDialogManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(app_frame));
}

views::Widget* FindModalDialogWidget() {
  for (views::Widget* widget : views::test::WidgetTest::GetAllWidgets()) {
    if (widget->widget_delegate() &&
        widget->widget_delegate()->AsDialogDelegate()) {
      return widget;
    }
  }
  return nullptr;
}

}  // namespace

class ControlledFrameClientCertTest : public ControlledFrameTestBase {
 public:
  void SetUpOnMainThread() override {
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ControlledFrameTestBase::SetUpOnMainThread();

    ProfileNetworkContextServiceFactory::GetForContext(profile())
        ->set_client_cert_store_factory_for_testing(
            base::BindRepeating(&CreateCertStore));

    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    client_cert_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                     ssl_config);
    client_cert_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(client_cert_server_.Start());
  }

 protected:
  GURL GetClientCertUrl() const {
    static constexpr std::string_view kClientCertRelativeUrl =
        "/ssl/browser_use_client_cert_store.html";
    return client_cert_server_.GetURL(kClientCertRelativeUrl);
  }

  net::EmbeddedTestServer client_cert_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
};

// Verifies that AutoSelectCertificateForUrls policy automatically supplies
// client certificates to requests originating from a <controlledframe> inside
// an IWA.
IN_PROC_BROWSER_TEST_F(ControlledFrameClientCertTest, AutoSelectCertificate) {
  const GURL client_cert_url = GetClientCertUrl();
  SetAutoSelectCertificateForUrl(profile(), client_cert_url);

  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/index.html");

  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  ASSERT_TRUE(web_view_guest);

  content::TestNavigationObserver navigation_observer(
      web_view_guest->web_contents(), /*expected_number_of_navigations=*/1u);
  EXPECT_TRUE(content::ExecJs(
      app_frame,
      content::JsReplace("document.querySelector('controlledframe').src = $1;",
                         client_cert_url)));
  navigation_observer.WaitForNavigationFinished();

  EXPECT_TRUE(content::WaitForLoadStop(web_view_guest->web_contents()));
  EXPECT_EQ(client_cert_url, web_view_guest->GetGuestMainFrame()
                                 ->GetLastCommittedURL()
                                 .GetWithoutRef());
  EXPECT_EQ(
      "pass",
      web_view_guest->GetGuestMainFrame()->GetLastCommittedURL().GetRef());

  auto* dialog_manager = GetModalDialogManager(app_frame);
  ASSERT_TRUE(dialog_manager);
  EXPECT_FALSE(dialog_manager->IsDialogActive());
}

// Verifies that a <controlledframe> client certificate request triggers the
// certificate picker dialog anchored to the parent IWA window. Accepting the
// dialog then completes the TLS client authentication.
IN_PROC_BROWSER_TEST_F(ControlledFrameClientCertTest,
                       CertificateSelectorForControlledFrame) {
  const GURL client_cert_url = GetClientCertUrl();

  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/index.html");

  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  ASSERT_TRUE(web_view_guest);

  base::RunLoop run_loop;
  ClientCertStoreStub::SetQuitClosure(run_loop.QuitClosure());

  EXPECT_TRUE(content::ExecJs(
      app_frame,
      content::JsReplace("document.querySelector('controlledframe').src = $1;",
                         client_cert_url)));
  run_loop.Run();

  auto* dialog_manager = GetModalDialogManager(app_frame);
  ASSERT_TRUE(dialog_manager);
  EXPECT_TRUE(dialog_manager->IsDialogActive());

  views::Widget* dialog = FindModalDialogWidget();
  ASSERT_TRUE(dialog);

  content::TestNavigationObserver navigation_observer(
      web_view_guest->web_contents(), /*expected_number_of_navigations=*/1u);
  views::test::AcceptDialog(dialog);
  navigation_observer.WaitForNavigationFinished();

  EXPECT_TRUE(content::WaitForLoadStop(web_view_guest->web_contents()));
  EXPECT_EQ(client_cert_url, web_view_guest->GetGuestMainFrame()
                                 ->GetLastCommittedURL()
                                 .GetWithoutRef());
  EXPECT_EQ(
      "pass",
      web_view_guest->GetGuestMainFrame()->GetLastCommittedURL().GetRef());
  EXPECT_FALSE(dialog_manager->IsDialogActive());
}

// Verifies that canceling the certificate picker dialog dismisses the prompt
// without sending a client certificate.
IN_PROC_BROWSER_TEST_F(ControlledFrameClientCertTest,
                       CancelCertificateSelectorForControlledFrame) {
  const GURL client_cert_url = GetClientCertUrl();

  auto [app_frame, controlled_frame] =
      InstallAndOpenIwaThenCreateControlledFrame(
          /*controlled_frame_host_name=*/std::nullopt, "/index.html");

  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  ASSERT_TRUE(web_view_guest);

  base::RunLoop run_loop;
  ClientCertStoreStub::SetQuitClosure(run_loop.QuitClosure());

  EXPECT_TRUE(content::ExecJs(
      app_frame,
      content::JsReplace("document.querySelector('controlledframe').src = $1;",
                         client_cert_url)));
  run_loop.Run();

  auto* dialog_manager = GetModalDialogManager(app_frame);
  ASSERT_TRUE(dialog_manager);
  EXPECT_TRUE(dialog_manager->IsDialogActive());

  views::Widget* dialog = FindModalDialogWidget();
  ASSERT_TRUE(dialog);

  content::TestNavigationObserver navigation_observer(
      web_view_guest->web_contents(), /*expected_number_of_navigations=*/1u);
  views::test::CancelDialog(dialog);
  navigation_observer.WaitForNavigationFinished();

  EXPECT_FALSE(dialog_manager->IsDialogActive());
  EXPECT_NE(
      "pass",
      web_view_guest->GetGuestMainFrame()->GetLastCommittedURL().GetRef());
}

}  // namespace controlled_frame
