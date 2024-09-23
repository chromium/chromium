// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_STORE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_STORE_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace android_webview {

namespace prefs {

// List of profile dictionaries.
constexpr char kProfileListPref[] = "profile.list";

// Counter for assigning profile numbers (without reuse). Stores the last
// assigned number, or 0 if never used, which is the same as the total number of
// non-default profiles ever created.
constexpr char kProfileCounterPref[] = "profile.counter";

}  // namespace prefs

class AwBrowserContext;

// AwBrowserContextStore is the container for AwBrowserContexts.
//
// - Owns and manages the lifetimes of instantiated AwBrowserContexts
//   ("Profiles").
//
// - Keeps track of the profiles instantiated into memory and the profiles saved
//   to disk (via the local_state pref_service), including mappings between
//   profile names and directory names.
//
// Most profiles are created and instantiated lazily (via the Get method).
// However, there is a special "Default" profile which is always created and
// instantiated on startup.
// Lifetime: Singleton
class AwBrowserContextStore final {
 public:
  static constexpr char kDefaultContextName[] = "Default";
  static constexpr char kDefaultContextPath[] = "Default";

  enum class DeletionResult {
    kDeleted,
    kDoesNotExist,
    kInUse,
  };

  ~AwBrowserContextStore() = delete;
  AwBrowserContextStore(const AwBrowserContextStore&) = delete;
  AwBrowserContextStore& operator=(const AwBrowserContextStore&) = delete;

  // Initialize the store if needed and then get a pointer to it.
  static AwBrowserContextStore* GetOrCreateInstance();
  // CHECK the store is initialized and then get a pointer to it.
  static AwBrowserContextStore* GetInstance();

  // Get the default context. This will never return null.
  AwBrowserContext* GetDefault() const;
  // Check if a named context exists (on disk or in memory). The context does
  // not need to be instantiated. The result is given to the callback once
  // determined.
  bool Exists(const std::string& name) const;
  // Instantiate and get the context with the given name. If the context does
  // not already exist (on disk or in memory), it is created if create_if_needed
  // is true, otherwise no context is instantiated and nullptr is returned.
  AwBrowserContext* Get(const std::string& name, bool create_if_needed);
  // Get a list of all contexts' names (on disk and in memory).
  std::vector<std::string> List() const;
  // Delete the named context if possible.
  DeletionResult Delete(const std::string& name);

  // Get the relative path for the named context. The context must exist (on
  // disk or in memory), but does not need to be instantiated.
  base::FilePath GetRelativePathForTesting(const std::string& name) const;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend class base::NoDestructor<AwBrowserContextStore>;

  struct Entry {
    Entry();
    explicit Entry(base::FilePath&& path,
                   std::unique_ptr<AwBrowserContext>&& instance);
    ~Entry();
    Entry(Entry&&);
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

    base::FilePath path;
    std::unique_ptr<AwBrowserContext> instance;
  };

  // Create the global store of all AwBrowserContexts. The default context is
  // pre-loaded into the store, which may involve blocking IO.
  explicit AwBrowserContextStore(PrefService* pref_service);

  // Registers a new (default or non-default) context with the given name and
  // ensures filesystem state is ready for the context to be
  // initialized. Returns a pointer to the entry inserted into contexts_. This
  // does not instantiate the context in-process.
  //
  // It is illegal to create a context which already exists.
  Entry* CreateNewContext(std::string_view name);
  // Finds and reserves a non-default profile number which hasn't previously
  // been assigned to any profile (even deleted ones).
  int AssignNewProfileNumber();

  // (local_state) PrefService which is used to persist information about the
  // profiles that exist on disk.
  const raw_ref<PrefService> prefs_;

  // Map of all existing (on disk and in memory) profiles, including ones which
  // haven't been instantiated.
  std::map<std::string, Entry> contexts_;
  // The default context is initialized during store construction. Once the
  // store is created, this pointer will be non-null.
  raw_ptr<AwBrowserContext> default_context_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_STORE_H_
