// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/window_type_launcher.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "ash/wm/test_child_modal_parent.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

using views::MdTextButton;
using views::MenuItemView;
using views::MenuRunner;

namespace ash {
namespace shell {

namespace {

constexpr int kWindowShadowElevation = 8;
constexpr const SkColor kColors[] = {SK_ColorRED, SK_ColorYELLOW, SK_ColorBLUE,
                                     SK_ColorGREEN};

int g_color_index = 0;

class ModalWindow : public views::WidgetDelegateView,
                    public views::ButtonListener {
 public:
  explicit ModalWindow(ui::ModalType modal_type)
      : modal_type_(modal_type), color_(kColors[g_color_index]) {
    ++g_color_index %= base::size(kColors);
    open_button_ =
        AddChildView(MdTextButton::Create(this, base::ASCIIToUTF16("Moar!")));
  }
  ~ModalWindow() override = default;

  static void OpenModalWindow(aura::Window* parent, ui::ModalType modal_type) {
    views::Widget* widget = views::Widget::CreateWindowWithParent(
        new ModalWindow(modal_type), parent);
    widget->GetNativeView()->SetName("ModalWindow");
    widget->Show();
  }

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(200, 200);
  }
  void Layout() override {
    gfx::Size open_ps = open_button_->GetPreferredSize();
    gfx::Rect local_bounds = GetLocalBounds();
    open_button_->SetBounds(5, local_bounds.bottom() - open_ps.height() - 5,
                            open_ps.width(), open_ps.height());
  }

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override { return true; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Modal Window");
  }
  ui::ModalType GetModalType() const override { return modal_type_; }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK(sender == open_button_);
    OpenModalWindow(GetWidget()->GetNativeView(), modal_type_);
  }

 private:
  ui::ModalType modal_type_;
  SkColor color_;
  views::Button* open_button_;

  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

class NonModalTransient : public views::WidgetDelegateView {
 public:
  NonModalTransient() : color_(kColors[g_color_index]) {
    ++g_color_index %= base::size(kColors);
  }
  ~NonModalTransient() override = default;

  static void OpenNonModalTransient(aura::Window* parent) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new NonModalTransient, parent);
    widget->GetNativeView()->SetName("NonModalTransient");
    widget->Show();
  }

  static void ToggleNonModalTransient(aura::Window* parent) {
    if (!non_modal_transient_) {
      non_modal_transient_ =
          views::Widget::CreateWindowWithParent(new NonModalTransient, parent);
      non_modal_transient_->GetNativeView()->SetName("NonModalTransient");
    }
    if (non_modal_transient_->IsVisible())
      non_modal_transient_->Hide();
    else
      non_modal_transient_->Show();
  }

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), color_);
  }
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(250, 250);
  }

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override { return true; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Non-Modal Transient");
  }
  void DeleteDelegate() override {
    if (GetWidget() == non_modal_transient_)
      non_modal_transient_ = nullptr;

    delete this;
  }

 private:
  SkColor color_;

  static views::Widget* non_modal_transient_;

  DISALLOW_COPY_AND_ASSIGN(NonModalTransient);
};

// static
views::Widget* NonModalTransient::non_modal_transient_ = nullptr;

template <class T>
T* AddViewToLayout(views::GridLayout* layout, std::unique_ptr<T> view) {
  layout->StartRow(0, 0);
  T* result = layout->AddView(std::move(view));
  layout->AddPaddingRow(0, 5);
  return result;
}

}  // namespace

void InitWindowTypeLauncher(
    base::RepeatingClosure show_views_examples_callback,
    base::RepeatingClosure create_embedded_browser_callback) {
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      new WindowTypeLauncher(show_views_examples_callback,
                             create_embedded_browser_callback),
      Shell::GetPrimaryRootWindow(), gfx::Rect(120, 120, 300, 410));
  widget->GetNativeView()->SetName("WindowTypeLauncher");
  ::wm::SetShadowElevation(widget->GetNativeView(), kWindowShadowElevation);
  widget->Show();
}

WindowTypeLauncher::WindowTypeLauncher(
    base::RepeatingClosure show_views_examples_callback,
    base::RepeatingClosure create_embedded_browser_callback)
    : show_views_examples_callback_(std::move(show_views_examples_callback)),
      create_embedded_browser_callback_(
          std::move(create_embedded_browser_callback)) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  SetBorder(views::CreateEmptyBorder(gfx::Insets(5)));
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);
  create_button_ = AddViewToLayout(
      layout, MdTextButton::Create(this, base::ASCIIToUTF16("Create Window")));
  create_nonresizable_button_ = AddViewToLayout(
      layout, MdTextButton::Create(
                  this, base::ASCIIToUTF16("Create Non-Resizable Window")));
  bubble_button_ = AddViewToLayout(
      layout,
      MdTextButton::Create(this, base::ASCIIToUTF16("Create Pointy Bubble")));
  lock_button_ = AddViewToLayout(
      layout, MdTextButton::Create(this, base::ASCIIToUTF16("Lock Screen")));
  widgets_button_ = AddViewToLayout(
      layout,
      MdTextButton::Create(this, base::ASCIIToUTF16("Show Example Widgets")));
  system_modal_button_ = AddViewToLayout(
      layout, MdTextButton::Create(
                  this, base::ASCIIToUTF16("Open System Modal Window")));
  window_modal_button_ = AddViewToLayout(
      layout, MdTextButton::Create(
                  this, base::ASCIIToUTF16("Open Window Modal Window")));
  child_modal_button_ = AddViewToLayout(
      layout, MdTextButton::Create(
                  this, base::ASCIIToUTF16("Open Child Modal Window")));
  transient_button_ = AddViewToLayout(
      layout, MdTextButton::Create(
                  this, base::ASCIIToUTF16("Open Non-Modal Transient Window")));
  examples_button_ = AddViewToLayout(
      layout, MdTextButton::Create(
                  this, base::ASCIIToUTF16("Open Views Examples Window")));
  show_hide_window_button_ = AddViewToLayout(
      layout,
      MdTextButton::Create(this, base::ASCIIToUTF16("Show/Hide a Window")));
  show_web_notification_ = AddViewToLayout(
      layout, MdTextButton::Create(
                  this, base::ASCIIToUTF16("Show a web/app notification")));
  embedded_browser_button_ = AddViewToLayout(
      layout,
      MdTextButton::Create(this, base::ASCIIToUTF16("Embedded Browser")));
  set_context_menu_controller(this);
}

WindowTypeLauncher::~WindowTypeLauncher() = default;

void WindowTypeLauncher::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), SK_ColorWHITE);
}

bool WindowTypeLauncher::OnMousePressed(const ui::MouseEvent& event) {
  // Overridden so we get OnMouseReleased and can show the context menu.
  return true;
}

bool WindowTypeLauncher::CanResize() const {
  return true;
}

base::string16 WindowTypeLauncher::GetWindowTitle() const {
  return base::ASCIIToUTF16("Examples: Window Builder");
}

bool WindowTypeLauncher::CanMaximize() const {
  return true;
}

bool WindowTypeLauncher::CanMinimize() const {
  return true;
}

void WindowTypeLauncher::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == create_button_) {
    ToplevelWindow::CreateParams params;
    params.can_resize = true;
    params.can_maximize = true;
    ToplevelWindow::CreateToplevelWindow(params);
  } else if (sender == create_nonresizable_button_) {
    ToplevelWindow::CreateToplevelWindow(ToplevelWindow::CreateParams());
  } else if (sender == embedded_browser_button_) {
    create_embedded_browser_callback_.Run();
  } else if (sender == bubble_button_) {
    CreatePointyBubble(sender);
  } else if (sender == lock_button_) {
    Shell::Get()->session_controller()->LockScreen();
  } else if (sender == widgets_button_) {
    CreateWidgetsWindow();
  } else if (sender == system_modal_button_) {
    ModalWindow::OpenModalWindow(GetWidget()->GetNativeView(),
                                 ui::MODAL_TYPE_SYSTEM);
  } else if (sender == window_modal_button_) {
    ModalWindow::OpenModalWindow(GetWidget()->GetNativeView(),
                                 ui::MODAL_TYPE_WINDOW);
  } else if (sender == child_modal_button_) {
    TestChildModalParent::Show(GetWidget()->GetNativeView()->GetRootWindow());
  } else if (sender == transient_button_) {
    NonModalTransient::OpenNonModalTransient(GetWidget()->GetNativeView());
  } else if (sender == show_hide_window_button_) {
    NonModalTransient::ToggleNonModalTransient(GetWidget()->GetNativeView());
  } else if (sender == show_web_notification_) {
    std::unique_ptr<message_center::Notification> notification =
        std::make_unique<message_center::Notification>(
            message_center::NOTIFICATION_TYPE_SIMPLE, "id0",
            base::ASCIIToUTF16("Test Shell Web Notification"),
            base::ASCIIToUTF16("Notification message body."), gfx::Image(),
            base::ASCIIToUTF16("www.testshell.org"), GURL(),
            message_center::NotifierId(
                message_center::NotifierType::APPLICATION, "test-id"),
            message_center::RichNotificationData(), nullptr /* delegate */);

    message_center::MessageCenter::Get()->AddNotification(
        std::move(notification));
  } else if (sender == examples_button_) {
    show_views_examples_callback_.Run();
  }
}

void WindowTypeLauncher::ExecuteCommand(int id, int event_flags) {
  switch (id) {
    case COMMAND_NEW_WINDOW:
      InitWindowTypeLauncher(show_views_examples_callback_,
                             create_embedded_browser_callback_);
      break;
    case COMMAND_TOGGLE_FULLSCREEN:
      GetWidget()->SetFullscreen(!GetWidget()->IsFullscreen());
      break;
    default:
      break;
  }
}

void WindowTypeLauncher::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  MenuItemView* root = new MenuItemView(this);
  root->AppendMenuItem(COMMAND_NEW_WINDOW, base::ASCIIToUTF16("New Window"));
  root->AppendMenuItem(COMMAND_TOGGLE_FULLSCREEN,
                       base::ASCIIToUTF16("Toggle FullScreen"));
  // MenuRunner takes ownership of root.
  menu_runner_.reset(new MenuRunner(
      root, MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

}  // namespace shell
}  // namespace ash
