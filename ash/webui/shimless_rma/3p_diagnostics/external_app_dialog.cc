// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/3p_diagnostics/external_app_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/file_select_listener.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace ash::shimless_rma {

namespace {

ExternalAppDialog* g_instance = nullptr;
base::RepeatingCallback<void(const ExternalAppDialog::InitParams&)>
    g_mock_show_function;

constexpr double kRelativeScreenWidth = 0.9;
constexpr double kRelativeScreenHeight = 0.8;

class WebContentsHandler
    : public ui::WebDialogWebContentsDelegate::WebContentsHandler {
 public:
  WebContentsHandler();
  WebContentsHandler(const WebContentsHandler&) = delete;
  WebContentsHandler& operator=(const WebContentsHandler&) = delete;
  ~WebContentsHandler() override;

  // WebDialogWebContentsDelegate::WebContentsHandler overrides:
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::BrowserContext* context,
                      content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
};

WebContentsHandler::WebContentsHandler() = default;

WebContentsHandler::~WebContentsHandler() = default;

content::WebContents* WebContentsHandler::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return nullptr;
}

void WebContentsHandler::AddNewContents(
    content::BrowserContext* context,
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {}

void WebContentsHandler::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  listener->FileSelectionCanceled();
}

std::string_view ConsoleMessageLevelToString(
    blink::mojom::ConsoleMessageLevel level) {
  switch (level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      return "VERBOSE";
    case blink::mojom::ConsoleMessageLevel::kInfo:
      return "INFO";
    case blink::mojom::ConsoleMessageLevel::kWarning:
      return "WARNING";
    case blink::mojom::ConsoleMessageLevel::kError:
      return "ERROR";
  }
}

}  // namespace

ExternalAppDialog::InitParams::InitParams() = default;

ExternalAppDialog::InitParams::~InitParams() = default;

// static
void ExternalAppDialog::Show(const InitParams& params) {
  if (g_instance) {
    LOG(ERROR) << "Can only show one ExternalAppDialog";
    return;
  }
  if (g_mock_show_function) {
    g_mock_show_function.Run(params);
    return;
  }
  new ExternalAppDialog(params);
}

// static
content::WebContents* ExternalAppDialog::GetWebContents() {
  return g_instance ? g_instance->web_dialog_view_->web_contents() : nullptr;
}

// static
void ExternalAppDialog::SetMockShowForTesting(
    base::RepeatingCallback<void(const InitParams& params)> callback) {
  g_mock_show_function = callback;
}

// static
void ExternalAppDialog::CloseForTesting() {
  if (g_instance) {
    g_instance->widget_->Close();
  }
}

ExternalAppDialog::ExternalAppDialog(const InitParams& params)
    : content::WebContentsObserver(nullptr),
      on_console_log_(params.on_console_log) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  set_can_close(true);
  set_can_resize(false);
  set_center_dialog_title_text(true);
  set_close_dialog_on_escape(false);
  set_dialog_content_url(params.content_url);
  set_dialog_modal_type(ui::MODAL_TYPE_SYSTEM);
  set_dialog_title(base::UTF8ToUTF16(params.app_name));

  views::Widget::InitParams widget_params{};
  widget_params.z_order = ui::ZOrderLevel::kFloatingWindow;
  web_dialog_view_ = new views::WebDialogView(
      params.context, this, std::make_unique<WebContentsHandler>());
  widget_params.delegate = web_dialog_view_;
  widget_params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_LockSystemModalContainer);

  widget_ = new views::Widget;
  widget_->Init(std::move(widget_params));
  widget_->Show();
}

ExternalAppDialog::~ExternalAppDialog() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void ExternalAppDialog::GetDialogSize(gfx::Size* size) const {
  gfx::Size screen_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  *size = gfx::Size(kRelativeScreenWidth * screen_size.width(),
                    kRelativeScreenHeight * screen_size.height());
}

void ExternalAppDialog::OnLoadingStateChanged(content::WebContents* source) {
  content::WebContentsObserver::Observe(source);
}

void ExternalAppDialog::OnDidAddMessageToConsole(
    content::RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  if (ash::features::IsShimlessRMA3pDiagnosticsDevModeEnabled()) {
    LOG(WARNING) << "[CONSOLE " << ConsoleMessageLevelToString(log_level)
                 << "] \"" << message << "\", source: " << source_id << " ("
                 << line_no << ")";
  }
  if (on_console_log_) {
    on_console_log_.Run(content::ConsoleMessageLevelToLogSeverity(log_level),
                        message, line_no, source_id);
  }
}

}  // namespace ash::shimless_rma
