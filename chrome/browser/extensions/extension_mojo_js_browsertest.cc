// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_mojo_binder_registry.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/keep_alive.mojom.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr char kRegisteredInterface[] = "extensions.KeepAlive";
constexpr char kUnregisteredInterface[] = "blink.mojom.CacheStorage";

// The extension gets its ID from the manifest `key`, which replaces %s.
constexpr char kManifestTemplate[] = R"({
  "name": "MojoJS test",
  "version": "1",
  "manifest_version": 3,
  "key": "%s",
  "sandbox": {"pages": ["sandboxed.html"]}
})";

// Enables MojoJS for one extension and registers `extensions.KeepAlive` as its
// only interface. The frame's BrowserInterfaceBroker serves that interface too.
// `on_bind` running shows that the request reached the extension's own broker.
class TestBinderProvider : public ExtensionMojoBinderProvider {
 public:
  using OnBindCallback =
      base::RepeatingCallback<void(content::RenderFrameHost*)>;

  TestBinderProvider(ExtensionId extension_id, OnBindCallback on_bind)
      : ExtensionMojoBinderProvider(std::move(extension_id)),
        on_bind_(std::move(on_bind)) {}

  bool IsMojoJsEnabledForFrame() const override { return true; }

  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension& extension) override {
    binder_map.Add<KeepAlive>(base::BindRepeating(
        [](const OnBindCallback& on_bind,
           content::RenderFrameHost* render_frame_host,
           mojo::PendingReceiver<KeepAlive>) {
          on_bind.Run(render_frame_host);
        },
        on_bind_));
  }

 private:
  OnBindCallback on_bind_;
};

// Returns a script that binds `interface_name` with `Mojo.bindInterface()` in
// `scope`, and evaluates to "ok" or to the name of the thrown exception.
std::string BindInterfaceScript(std::string_view interface_name,
                                std::string_view scope) {
  static constexpr char kScriptTemplate[] = R"(
    (() => {
      try {
        const {handle1} = Mojo.createMessagePipe();
        Mojo.bindInterface($1, handle1, $2);
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )";
  return content::JsReplace(kScriptTemplate, interface_name, scope);
}

}  // namespace

class ExtensionMojoJsBrowserTest : public ExtensionBrowserTest {
 protected:
  // Loads a component extension with two pages, a sandboxed page and a page
  // with a sandboxed iframe, and registers a provider that enables MojoJS for
  // it.
  void LoadExtensionWithMojoJs() {
    std::string key;
    ASSERT_TRUE(Extension::ProducePEM("extension_mojo_js_browsertest", &key));
    dir_.WriteManifest(base::StringPrintf(kManifestTemplate, key.c_str()));
    dir_.WriteFile(FILE_PATH_LITERAL("page1.html"), "<p>1</p>");
    dir_.WriteFile(FILE_PATH_LITERAL("page2.html"), "<p>2</p>");
    dir_.WriteFile(FILE_PATH_LITERAL("sandboxed.html"), "<p>sandboxed</p>");
    dir_.WriteFile(FILE_PATH_LITERAL("sandboxed_iframe.html"),
                   R"(<iframe sandbox="allow-scripts" src="page2.html">)"
                   "</iframe>");
    extension_ = LoadExtensionAsComponent(dir_.UnpackedPath());
    ASSERT_TRUE(extension_);

    ExtensionMojoBinderRegistryFactory::GetOrCreateForBrowserContext(profile())
        ->RegisterProvider(
            base::PassKey<ExtensionMojoJsBrowserTest>(),
            std::make_unique<TestBinderProvider>(
                extension_->id(), bind_future_.GetRepeatingCallback()));
  }

  content::RenderFrameHost* NavigateToExtensionPage(std::string_view page) {
    if (!content::NavigateToURL(GetActiveWebContents(),
                                extension_->GetResourceURL(page))) {
      return nullptr;
    }
    return GetActiveWebContents()->GetPrimaryMainFrame();
  }

  // Resolves with the RenderFrameHost that the provider's binder ran with.
  base::test::TestFuture<content::RenderFrameHost*>& bind_future() {
    return bind_future_;
  }

  const ExtensionId& extension_id() const { return extension_->id(); }

  void TearDownOnMainThread() override {
    extension_ = nullptr;
    ExtensionBrowserTest::TearDownOnMainThread();
  }

 private:
  TestExtensionDir dir_;
  raw_ptr<const Extension> extension_ = nullptr;
  base::test::TestFuture<content::RenderFrameHost*> bind_future_;
};

// MojoJS in an extension document only reaches the interfaces registered by
// the extension's provider. The second navigation checks that the next
// document in the same tab is given a broker too.
IN_PROC_BROWSER_TEST_F(ExtensionMojoJsBrowserTest,
                       BindsOnlyRegisteredInterfaces) {
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithMojoJs());

  for (const char* page : {"page1.html", "page2.html"}) {
    SCOPED_TRACE(page);
    content::RenderFrameHost* render_frame_host = NavigateToExtensionPage(page);
    ASSERT_TRUE(render_frame_host);

    EXPECT_EQ(true, content::EvalJs(render_frame_host,
                                    "typeof Mojo !== 'undefined'"));

    // Blink only allows the 'context' scope once a MojoJS broker is set, so
    // process-scoped interfaces are out of reach.
    EXPECT_EQ(
        "NotAllowedError",
        content::EvalJs(render_frame_host,
                        BindInterfaceScript(kRegisteredInterface, "process")));

    // A registered interface reaches the provider's binder, which runs with
    // the committed document's RenderFrameHost.
    EXPECT_EQ("ok", content::EvalJs(
                        render_frame_host,
                        BindInterfaceScript(kRegisteredInterface, "context")));
    EXPECT_EQ(render_frame_host, bind_future().Take());
  }
}

// Requesting an interface that the extension did not register is a bad
// message, even if the frame's BrowserInterfaceBroker would serve it.
IN_PROC_BROWSER_TEST_F(ExtensionMojoJsBrowserTest,
                       UnregisteredInterfaceIsBadMessage) {
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithMojoJs());
  content::RenderFrameHost* render_frame_host =
      NavigateToExtensionPage("page1.html");
  ASSERT_TRUE(render_frame_host);

  content::RenderProcessHostBadMojoMessageWaiter bad_message_waiter(
      render_frame_host->GetProcess());
  content::ExecuteScriptAsync(
      render_frame_host,
      BindInterfaceScript(kUnregisteredInterface, "context"));
  EXPECT_EQ(
      base::StrCat({"Received bad user message: No binder found for "
                    "interface ",
                    kUnregisteredInterface, " in the extension MojoJS broker"}),
      bad_message_waiter.Wait());

  // The next document in the tab, in a new renderer process, gets a working
  // broker. Launching that process also waits for the process launcher to
  // finish stopping the terminated one.
  render_frame_host = NavigateToExtensionPage("page1.html");
  ASSERT_TRUE(render_frame_host);
  EXPECT_EQ("ok", content::EvalJs(
                      render_frame_host,
                      BindInterfaceScript(kRegisteredInterface, "context")));
  EXPECT_EQ(render_frame_host, bind_future().Take());
}

// A sandboxed extension page commits an opaque origin, which does not belong to
// the extension, so it does not get MojoJS.
IN_PROC_BROWSER_TEST_F(ExtensionMojoJsBrowserTest, SandboxedPageHasNoMojoJs) {
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithMojoJs());
  content::RenderFrameHost* render_frame_host =
      NavigateToExtensionPage("sandboxed.html");
  ASSERT_TRUE(render_frame_host);
  // Only the origin, not the frame's SiteInstance, rules out the extension.
  ASSERT_EQ(extension_id(), util::GetExtensionIdFromFrame(render_frame_host));
  ASSERT_TRUE(render_frame_host->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(false,
            content::EvalJs(render_frame_host, "typeof Mojo !== 'undefined'"));
}

// A sandboxed iframe in an extension page also commits an opaque origin, so it
// does not get MojoJS, while its embedder does.
IN_PROC_BROWSER_TEST_F(ExtensionMojoJsBrowserTest, SandboxedIframeHasNoMojoJs) {
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithMojoJs());
  content::RenderFrameHost* render_frame_host =
      NavigateToExtensionPage("sandboxed_iframe.html");
  ASSERT_TRUE(render_frame_host);
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(render_frame_host, 0);
  ASSERT_TRUE(child_frame);
  // Only the origin, not the frame's SiteInstance, rules out the extension.
  ASSERT_EQ(extension_id(), util::GetExtensionIdFromFrame(child_frame));
  ASSERT_TRUE(child_frame->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(true,
            content::EvalJs(render_frame_host, "typeof Mojo !== 'undefined'"));
#if !BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, component extensions can use mojoPrivate, so the extensions
  // renderer enables MojoJS in the iframe itself.
  EXPECT_EQ(false, content::EvalJs(child_frame, "typeof Mojo !== 'undefined'"));
#endif
}

}  // namespace extensions
