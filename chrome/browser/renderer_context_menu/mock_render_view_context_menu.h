// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_MOCK_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_MOCK_RENDER_VIEW_CONTEXT_MENU_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"

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
    std::u16string title;
    ui::ImageModel icon;
  };

  explicit MockRenderViewContextMenu(bool incognito);

  MockRenderViewContextMenu(const MockRenderViewContextMenu&) = delete;
  MockRenderViewContextMenu& operator=(const MockRenderViewContextMenu&) =
      delete;

  ~MockRenderViewContextMenu() override;

  // SimpleMenuModel::Delegate implementation.
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // RenderViewContextMenuProxy implementation.
  void AddMenuItem(int command_id, const std::u16string& title) override;
  void AddMenuItemWithIcon(int command_id,
                           const std::u16string& title,
                           const ui::ImageModel& icon) override;
  void AddCheckItem(int command_id, const std::u16string& title) override;
  void AddSeparator() override;
  void AddSubMenu(int command_id,
                  const std::u16string& label,
                  ui::MenuModel* model) override;
  void AddSubMenuWithStringIdAndIcon(int command_id,
                                     int message_id,
                                     ui::MenuModel* model,
                                     const ui::ImageModel& icon) override;
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const std::u16string& title) override;
  void UpdateMenuIcon(int command_id, const ui::ImageModel& icon) override;
  void RemoveMenuItem(int command_id) override;
  void RemoveAdjacentSeparators() override;
  void RemoveSeparatorBeforeMenuItem(int command_id) override;
  void AddSpellCheckServiceItem(bool is_checked) override;
  void AddAccessibilityLabelsServiceItem(bool is_checked) override;
  content::RenderFrameHost* GetRenderFrameHost() const override;
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
  raw_ptr<RenderViewContextMenuObserver, DanglingUntriaged> observer_;

  // A dummy profile used in this test. Call GetPrefs() when a test needs to
  // change this profile and use PrefService methods.
  std::unique_ptr<TestingProfile> original_profile_;

  // Either |original_profile_| or its incognito profile.
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // The WebContents returned by GetWebContents(). This is owned by our owner
  // and the owner is responsible for its lifetime.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;

  // A list of menu items added.
  std::vector<MockMenuItem> items_;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_MOCK_RENDER_VIEW_CONTEXT_MENU_H_
