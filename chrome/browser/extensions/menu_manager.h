// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MENU_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_MENU_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/url_pattern_set.h"
#include "ui/gfx/image/image.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
struct ContextMenuParams;
}

namespace extensions {
class Extension;
class ExtensionMenuIconLoader;
class MenuIconLoader;
class StateStore;

// Represents a menu item added by an extension.
class MenuItem {
 public:
  using List = std::vector<MenuItem*>;
  using OwnedList = std::vector<std::unique_ptr<MenuItem>>;

  // Key used to identify which extension a menu item belongs to.  A menu item
  // can also belong to a <webview>, in which case |webview_embedder_process_id|
  // |webview_embedder_frame_id|, and |webview_instance_id| will be non-zero.
  // When two ExtensionKeys are compared, an empty |extension_id| will match any
  // other extension ID. This allows menu items belonging to webviews to be
  // found with only the |webview_embedder_process_id| and |webview_instance_id|
  // when the extension ID is not known. This is currently done from
  // ChromeExtensionsBrowserClient::CleanUpWebView(). The
  // |webview_embedder_frame_id| is only used to get the <webview>'s embedder
  // RenderFrameHost.
  struct ExtensionKey {
    std::string extension_id;
    int webview_embedder_process_id;
    int webview_embedder_frame_id;
    int webview_instance_id;

    ExtensionKey();
    explicit ExtensionKey(const std::string& extension_id);
    ExtensionKey(const std::string& extension_id,
                 int webview_embedder_process_id,
                 int webview_embedder_frame_id,
                 int webview_instance_id);

    bool operator==(const ExtensionKey& other) const;
    bool operator!=(const ExtensionKey& other) const;
    bool operator<(const ExtensionKey& other) const;

    bool empty() const;
  };

  // An Id uniquely identifies a context menu item registered by an extension.
  struct Id {
    Id();
    // Since the unique ID (uid or string_uid) is parsed from API arguments,
    // the normal usage is to set the uid or string_uid immediately after
    // construction.
    Id(bool incognito, const ExtensionKey& extension_key);
    ~Id();

    bool operator==(const Id& other) const;
    bool operator!=(const Id& other) const;
    bool operator<(const Id& other) const;

    bool incognito;
    ExtensionKey extension_key;
    // Only one of uid or string_uid will be defined.
    int uid;
    std::string string_uid;
  };

  // For context menus, these are the contexts where an item can appear.
  enum Context {
    ALL = 1,
    PAGE = 2,
    SELECTION = 4,
    LINK = 8,
    EDITABLE = 16,
    IMAGE = 32,
    VIDEO = 64,
    AUDIO = 128,
    FRAME = 256,
    LAUNCHER = 512,
    BROWSER_ACTION = 1024,
    PAGE_ACTION = 2048,
    ACTION = 4096,
  };

  // An item can be only one of these types.
  enum Type {
    NORMAL,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };

  // A list of Contexts for an item.
  class ContextList {
   public:
    ContextList() : value_(0) {}
    explicit ContextList(Context context) : value_(context) {}
    ContextList(const ContextList& other) : value_(other.value_) {}

    void operator=(const ContextList& other) {
      value_ = other.value_;
    }

    bool operator==(const ContextList& other) const {
      return value_ == other.value_;
    }

    bool operator!=(const ContextList& other) const {
      return !(*this == other);
    }

    bool Contains(Context context) const {
      return (value_ & context) > 0;
    }

    void Add(Context context) {
      value_ |= context;
    }

    base::Value ToValue() const {
      return base::Value(static_cast<int>(value_));
    }

    bool Populate(const base::Value& value) {
      if (!value.is_int() || value.GetInt() < 0)
        return false;
      value_ = value.GetInt();
      return true;
    }

   private:
    uint32_t value_;  // A bitmask of Context values.
  };

  MenuItem(const Id& id,
           const std::string& title,
           bool checked,
           bool visible,
           bool enabled,
           Type type,
           const ContextList& contexts);

  MenuItem(const MenuItem&) = delete;
  MenuItem& operator=(const MenuItem&) = delete;

  virtual ~MenuItem();

  // Simple accessor methods.
  bool incognito() const { return id_.incognito; }
  const std::string& extension_id() const {
    return id_.extension_key.extension_id;
  }
  const std::string& title() const { return title_; }
  const OwnedList& children() { return children_; }
  const Id& id() const { return id_; }
  Id* parent_id() const { return parent_id_.get(); }
  const ContextList& contexts() const { return contexts_; }
  Type type() const { return type_; }
  bool checked() const { return checked_; }
  bool visible() const { return visible_; }
  bool enabled() const { return enabled_; }
  const URLPatternSet& document_url_patterns() const {
    return document_url_patterns_;
  }
  const URLPatternSet& target_url_patterns() const {
    return target_url_patterns_;
  }

  // Simple mutator methods.
  void set_title(const std::string& new_title) { title_ = new_title; }
  void set_contexts(ContextList contexts) { contexts_ = contexts; }
  void set_type(Type type) { type_ = type; }
  void set_visible(bool visible) { visible_ = visible; }
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void set_document_url_patterns(const URLPatternSet& patterns) {
    document_url_patterns_ = patterns.Clone();
  }
  void set_target_url_patterns(const URLPatternSet& patterns) {
    target_url_patterns_ = patterns.Clone();
  }

  // Returns the title with any instances of %s replaced by |selection|. The
  // result will be no longer than |max_length|.
  std::u16string TitleWithReplacement(const std::u16string& selection,
                                      size_t max_length) const;

  // Sets the checked state to |checked|. Returns true if successful.
  bool SetChecked(bool checked);

  // Converts to Value for serialization to preferences.
  base::Value::Dict ToValue() const;

  // Returns a new MenuItem created from |value|, or NULL if there is
  // an error.
  static std::unique_ptr<MenuItem> Populate(const std::string& extension_id,
                                            const base::Value::Dict& value,
                                            std::string* error);

  // Sets any document and target URL patterns from |properties|.
  bool PopulateURLPatterns(
      const std::vector<std::string>* document_url_patterns,
      const std::vector<std::string>* target_url_patterns,
      std::string* error);

 protected:
  friend class MenuManager;

  // Adds |item| and sets its parent_id_.
  void AddChild(std::unique_ptr<MenuItem> item);

  // Removes the child item from this parent and returns it.
  std::unique_ptr<MenuItem> ReleaseChild(const Id& child_id, bool recursive);

  // Recursively appends all descendant items (children, grandchildren, etc.)
  // to the output |list|.
  void GetFlattenedSubtree(MenuItem::List* list);

  // Recursively removes all descendant items (children, grandchildren, etc.),
  // returning the ids of the removed items.
  std::set<Id> RemoveAllDescendants();

 private:
  // The unique id for this item.
  Id id_;

  // What gets shown in the menu for this item.
  std::string title_;

  Type type_;

  // This should only be true for items of type CHECKBOX or RADIO.
  bool checked_;

  // If the item is visible (shown or hidden) in the menu.
  bool visible_;

  // If the item is enabled or not.
  bool enabled_;

  // In what contexts should the item be shown?
  ContextList contexts_;

  // If this item is a child of another item, the unique id of its parent. If
  // this is a top-level item with no parent, this will be NULL.
  std::unique_ptr<Id> parent_id_;

  // Patterns for restricting what documents this item will appear for. This
  // applies to the frame where the click took place.
  URLPatternSet document_url_patterns_;

  // Patterns for restricting where items appear based on the src/href
  // attribute of IMAGE/AUDIO/VIDEO/LINK tags.
  URLPatternSet target_url_patterns_;

  // Any children this item may have.
  OwnedList children_;
};

// This class keeps track of menu items added by extensions.
class MenuManager : public ProfileObserver,
                    public KeyedService,
                    public ExtensionRegistryObserver {
 public:
  static const char kOnContextMenus[];
  static const char kOnWebviewContextMenus[];
  static constexpr MenuItem::OwnedList::size_type kMaxItemsPerExtension = 1000;

  class TestObserver : public base::CheckedObserver {
   public:
    ~TestObserver() override = default;
    virtual void DidReadFromStorage(const std::string& extension_id) {}
    virtual void WillWriteToStorage(const std::string& extension_id) {}
  };

  MenuManager(content::BrowserContext* context, StateStore* store_);

  MenuManager(const MenuManager&) = delete;
  MenuManager& operator=(const MenuManager&) = delete;

  ~MenuManager() override;

  // Convenience function to get the MenuManager for a browser context.
  static MenuManager* Get(content::BrowserContext* context);

  // Returns the keys of extensions which have menu items registered.
  std::set<MenuItem::ExtensionKey> ExtensionIds();

  // Returns a list of all the *top-level* menu items (added via AddContextItem)
  // for the given extension specified by |extension_key|, *not* including child
  // items (added via AddChildItem); although those can be reached via the
  // top-level items' children. A view can then decide how to display these,
  // including whether to put them into a submenu if there are more than 1.
  const MenuItem::OwnedList* MenuItems(
      const MenuItem::ExtensionKey& extension_key) const;

  // Returns the number of menu items for extension specified by
  // `extension_key`.
  MenuItem::OwnedList::size_type MenuItemsSize(
      const MenuItem::ExtensionKey& extension_key) const {
    const MenuItem::OwnedList* list = MenuItems(extension_key);
    return list ? list->size() : 0;
  }

  // Adds a top-level menu item for an extension, requiring the |extension|
  // pointer so it can load the icon for the extension. Returns a boolean
  // indicating success or failure.
  bool AddContextItem(const Extension* extension,
                      std::unique_ptr<MenuItem> item);

  // Add an item as a child of another item which has been previously added.
  // Returns a boolean indicating success or failure.
  bool AddChildItem(const MenuItem::Id& parent_id,
                    std::unique_ptr<MenuItem> child);

  // Makes existing item with |child_id| a child of the item with |parent_id|.
  // If the child item was already a child of another parent, this will remove
  // it from that parent first. It is an error to try and move an item to be a
  // child of one of its own descendants. It is legal to pass NULL for
  // |parent_id|, which means the item should be moved to the top-level.
  bool ChangeParent(const MenuItem::Id& child_id,
                    const MenuItem::Id* parent_id);

  // Removes a context menu item with the given id (whether it is a top-level
  // item or a child of some other item), returning true if the item was found
  // and removed or false otherwise.
  bool RemoveContextMenuItem(const MenuItem::Id& id);

  // Removes all items for the given extension specified by |extension_key|.
  void RemoveAllContextItems(const MenuItem::ExtensionKey& extension_key);

  // Returns the item with the given |id| or NULL.
  MenuItem* GetItemById(const MenuItem::Id& id) const;

  // Notify the MenuManager that an item has been updated not through
  // an explicit call into MenuManager. For example, if an item is
  // acquired by a call to GetItemById and changed, then this should be called.
  // Returns true if the item was found or false otherwise.
  bool ItemUpdated(const MenuItem::Id& id);

  // Called when a menu item is clicked on by the user.
  void ExecuteCommand(content::BrowserContext* context,
                      content::WebContents* web_contents,
                      content::RenderFrameHost* render_frame_host,
                      const content::ContextMenuParams& params,
                      const MenuItem::Id& menu_item_id);

  // This returns a image of width/height kFaviconSize, loaded through the
  // MenuIconLoader associated with the |extension_key|.
  gfx::Image GetIconForExtensionKey(
      const MenuItem::ExtensionKey& extension_key);

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Stores the menu items for the extension in the state storage. The write
  // will happen asynchronously after some delay.
  void WriteToStorage(const Extension* extension,
                      const MenuItem::ExtensionKey& extension_key);

  // Reads menu items for the extension from the state storage. Any invalid
  // items are ignored.
  void ReadFromStorage(const std::string& extension_id,
                       std::optional<base::Value> value);

  // Removes all "incognito" "split" mode context items.
  void RemoveAllIncognitoContextItems();

  // Associates |extension_key| with the given |menu_icon_loader|.
  void SetMenuIconLoader(MenuItem::ExtensionKey extension_key,
                         std::unique_ptr<MenuIconLoader> menu_icon_loader);

  // Returns the MenuIconLoader associated with |extension_key|.
  MenuIconLoader* GetMenuIconLoader(MenuItem::ExtensionKey extension_key);

  void AddObserver(TestObserver* observer);
  void RemoveObserver(TestObserver* observer);

 private:
  FRIEND_TEST_ALL_PREFIXES(MenuManagerTest, DeleteParent);
  FRIEND_TEST_ALL_PREFIXES(MenuManagerTest, RemoveOneByOne);

  // This is a helper function which takes care of de-selecting any other radio
  // items in the same group (i.e. that are adjacent in the list).
  void RadioItemSelected(MenuItem* item);

  // Make sure that there is only one radio item selected at once in any run.
  // If there are no radio items selected, then the first item in the run
  // will get selected. If there are multiple radio items selected, then only
  // the last one will get selected.
  void SanitizeRadioListsInMenu(const MenuItem::OwnedList& item_list);

  // Returns true if item is a descendant of an item with id |ancestor_id|.
  bool DescendantOf(MenuItem* item, const MenuItem::Id& ancestor_id);

  void WriteToStorageInternal(const MenuItem::ExtensionKey& extension_key);

  // We keep items organized by mapping ExtensionKey to a list of items.
  std::map<MenuItem::ExtensionKey, MenuItem::OwnedList> context_items_;

  // This lets us make lookup by id fast. It maps id to MenuItem* for
  // all items the menu manager knows about, including all children of top-level
  // items.
  std::map<MenuItem::Id, raw_ptr<MenuItem, CtnExperimental>> items_by_id_;

  // The scheduled tasks to write the menu items to storage.
  std::map<MenuItem::ExtensionKey, base::OneShotTimer> write_tasks_;

  // Listen to extension load, unloaded events.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};

  // Holds the default MenuIconLoader to use for extensions Context Menus API.
  std::unique_ptr<ExtensionMenuIconLoader> extension_menu_icon_loader_;

  // We keep a map of ExtensionKey to the MenuIconLoader that should be used to
  // load that context's menu icon for a WebView Context Menus API use.
  std::map<MenuItem::ExtensionKey, std::unique_ptr<MenuIconLoader>>
      webview_menu_icon_loaders_;

  raw_ptr<content::BrowserContext> browser_context_;

  // Owned by ExtensionSystem.
  raw_ptr<StateStore> store_;

  base::ObserverList<TestObserver> observers_;

  base::WeakPtrFactory<MenuManager> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MENU_MANAGER_H_
