// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_MODAL_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_MODAL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#else
namespace views {
class View;
}  // namespace views
#endif

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Platform-agnostic tab-scoped WebUI modal/sheet utility.
//
// - On Desktop (`!BUILDFLAG(IS_ANDROID)`), displays a `views::DialogDelegate`
//   wrapping a `views::WebView` via `tabs::TabDialogManager`.
// - On Android (`BUILDFLAG(IS_ANDROID)`), displays a `ThinWebView` inside a
//   centered `ModalDialogManager` dialog on tablet/desktop windows (`>=600dp`)
//   or inside a `BottomSheetController` modal bottom sheet on phone/compact
//   windows (`<600dp`), dynamically swapping containers across the `600dp`
//   breakpoint without reloading `WebContents`.
class GlicModal {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDialogElementId);

  struct Params {
    Params();
    Params(Params&&);
    Params& operator=(Params&&);
    ~Params();

    // Target WebUI URL to load inside the modal's WebContents.
    GURL content_url;

    // Desktop auto-resize bounds (used when !BUILDFLAG(IS_ANDROID)).
    gfx::Size initial_size = gfx::Size(512, 412);
    gfx::Size min_size = gfx::Size(512, 200);
    gfx::Size max_size = gfx::Size(512, 800);

    // Optional native loader view displayed while the WebUI / embedded site is
    // loading in the background. When provided, the underlying WebView is kept
    // hidden (alpha=0f on Android, covered in a FillLayout on Desktop) so Blink
    // layout and auto-resize still run, and revealed when `ShowContent()` is
    // called. When null/empty, the WebView is shown immediately.
#if !BUILDFLAG(IS_ANDROID)
    std::unique_ptr<views::View> loading_view;
#else
    base::android::ScopedJavaGlobalRef<jobject> loading_view;
#endif

    // When true, enables Blink auto-resize on the modal WebContents
    // (`EnableSizingFromWebContents` on Desktop, `rwhv->EnableAutoResize` +
    // container height wrapping on Android) so the modal dynamically sizes
    // to fit the WebUI / embedded `<webview>` content bounded by `min_size`
    // and `max_size` (or the container's available size on Android).
    bool size_to_content = false;

    // Optional callback invoked whenever the modal container's maximum
    // available content size (in DIP) is measured or changes (e.g., initial
    // layout, orientation change, or live <600dp <-> >=600dp container swap).
    base::RepeatingCallback<void(gfx::Size)> on_available_size_changed;

    // Suppresses the initial premature Blink auto-resize layout pass before the
    // WebUI's async content (e.g. embedded <webview>) finishes rendering.
    bool suppress_first_auto_resize = false;

    // Android portrait & landscape max dimensions in dp (used when
    // BUILDFLAG(IS_ANDROID)).
    gfx::Size portrait_max_size_dp = gfx::Size(380, 567);
    gfx::Size landscape_max_size_dp = gfx::Size(600, 310);

    // Whether to show a native overlay close button on Android.
    bool show_android_close_button = false;

    // Whether the WebView should be set as the initially focused view and
    // requested to focus when the primary main frame finishes loading.
    bool focus_web_view_on_open = true;

    // Optional accessible title announced when the modal becomes visible.
    std::u16string accessible_title;

    // Optional ElementIdentifier for InteractiveBrowserTest targeting.
    // Defaults to `GlicModal::kDialogElementId`.
    ui::ElementIdentifier element_identifier;

    // Invoked synchronously after the modal WebContents is created and initial
    // navigation has started, allowing the caller to initialize the
    // WebUIController (e.g. `SkillsUI::InitializeDialog`).
    base::OnceCallback<void(content::WebContents*)> on_webui_created;

    // Invoked when the modal is closed (either programmatically via `Close()`
    // or dismissed by the user).
    base::OnceClosure on_closed;
  };

  // Creates and shows a WebUI modal anchored to `tab`. Returns nullptr if `tab`
  // cannot currently host a modal (e.g., detached tab or missing window). If
  // `Show` returns nullptr, `params.on_closed` is NOT run by `Show`; callers
  // should handle a null return directly.
  // Destroying the returned `GlicModal` closes the modal without invoking
  // `on_closed`.
  static std::unique_ptr<GlicModal> Show(tabs::TabInterface& tab,
                                         Params params);

  GlicModal(const GlicModal&) = delete;
  GlicModal& operator=(const GlicModal&) = delete;
  virtual ~GlicModal() = default;

  // Programmatically closes the modal and triggers `Params::on_closed`.
  virtual void Close() = 0;

  // Ensures the modal is visible and active if the hosting tab is in the
  // foreground.
  virtual void Activate() = 0;

  // Reveals the underlying WebView content and removes the native loader view
  // (if one was provided in `Params`). Safe no-op if no loader view was
  // provided or if `ShowContent()` was already called.
  virtual void ShowContent() = 0;

  // Returns the hosted WebUI WebContents, or nullptr if already closed.
  virtual content::WebContents* GetWebContents() const = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual void SimulateDismissingForTesting() = 0;
#endif

 protected:
  GlicModal() = default;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_MODAL_H_
