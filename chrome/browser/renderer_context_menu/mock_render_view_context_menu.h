// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_MOCK_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_MOCK_RENDER_VIEW_CONTEXT_MENU_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image.h"

class PrefService;
class Profile;
class RenderViewContextMenuObserver;
class TestingProfile;

// A mock context menu proxy used in tests. This class overrides virtual methods
// derived from the RenderViewContextMenuProxy class to monitor calls from a
// MenuObserver class.
class MockRenderViewContextMenu : public ui::SimpleMenuModel::Delegate,
                                  public RenderViewContextMenuProxy {
 public:
  // A menu item used in this test.
  struct MockMenuItem {
    MockMenuItem();
    MockMenuItem(const MockMenuItem& other);
    ~MockMenuItem();

    MockMenuItem& operator=(const MockMenuItem& other);

    int command_id;
    bool enabled;
    bool checked;
    bool hidden;
    base::string16 title;
    gfx::Image icon;
  };

  explicit MockRenderViewContextMenu(bool incognito);
  ~MockRenderViewContextMenu() override;

  // SimpleMenuModel::Delegate implementation.
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // RenderViewContextMenuProxy implementation.
  void AddMenuItem(int command_id, const base::string16& title) override;
  void AddMenuItemWithIcon(int command_id,
                           const base::string16& title,
                           const gfx::ImageSkia& image) override;
  void AddMenuItemWithIcon(int command_id,
                           const base::string16& title,
                           const gfx::VectorIcon& icon) override;
  void AddCheckItem(int command_id, const base::string16& title) override;
  void AddSeparator() override;
  void AddSubMenu(int command_id,
                  const base::string16& label,
                  ui::MenuModel* model) override;
  void AddSubMenuWithStringIdAndIcon(int command_id,
                                     int message_id,
                                     ui::MenuModel* model,
                                     const gfx::ImageSkia& image) override;
  void AddSubMenuWithStringIdAndIcon(int command_id,
                                     int message_id,
                                     ui::MenuModel* model,
                                     const gfx::VectorIcon& icon) override;
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const base::string16& title) override;
  void UpdateMenuIcon(int command_id, const gfx::Image& image) override;
  void RemoveMenuItem(int command_id) override;
  void RemoveAdjacentSeparators() override;
  void AddSpellCheckServiceItem(bool is_checked) override;
  void AddAccessibilityLabelsServiceItem(bool is_checked) override;
  content::RenderViewHost* GetRenderViewHost() const override;
  content::BrowserContext* GetBrowserContext() const override;
  content::WebContents* GetWebContents() const override;

  // Attaches a RenderViewContextMenuObserver to be tested.
  void SetObserver(RenderViewContextMenuObserver* observer);

  // Returns the number of items added by the test.
  size_t GetMenuSize() const;

  // Returns the item at |index|.
  bool GetMenuItem(size_t index, MockMenuItem* item) const;

  // Returns the writable profile used in this test.
  PrefService* GetPrefs();

  // Sets a WebContents to be returned by GetWebContents().
  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  // Helper function to append items in sub menu from |model|.
  void AppendSubMenuItems(ui::MenuModel* model);

  // An observer used for initializing the status of menu items added in this
  // test. This is owned by our owner and the owner is responsible for its
  // lifetime.
  RenderViewContextMenuObserver* observer_;

  // A dummy profile used in this test. Call GetPrefs() when a test needs to
  // change this profile and use PrefService methods.
  std::unique_ptr<TestingProfile> original_profile_;

  // Either |original_profile_| or its incognito profile.
  Profile* profile_;

  // The WebContents returned by GetWebContents(). This is owned by our owner
  // and the owner is responsible for its lifetime.
  content::WebContents* web_contents_ = nullptr;

  // A list of menu items added.
  std::vector<MockMenuItem> items_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderViewContextMenu);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_MOCK_RENDER_VIEW_CONTEXT_MENU_H_
