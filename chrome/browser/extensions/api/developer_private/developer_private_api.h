// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_event_router.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/pref_types.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/clipboard/file_info.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace extensions {

// Key that indicates whether the safety check warning for this
// extension has been acknowledged because the user has chosen to keep
// it in a past review.
inline constexpr PrefMap kPrefAcknowledgeSafetyCheckWarningReason = {
    "ack_safety_check_warning_reason", PrefType::kInteger,
    PrefScope::kExtensionSpecific};

class EventRouter;

// The profile-keyed service that manages the DeveloperPrivate API.
class DeveloperPrivateAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer {
 public:
  using UnpackedRetryId = std::string;

  static BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
  GetFactoryInstance();

  // Convenience method to get the DeveloperPrivateAPI for a profile.
  static DeveloperPrivateAPI* Get(content::BrowserContext* context);

  explicit DeveloperPrivateAPI(content::BrowserContext* context);

  DeveloperPrivateAPI(const DeveloperPrivateAPI&) = delete;
  DeveloperPrivateAPI& operator=(const DeveloperPrivateAPI&) = delete;

  ~DeveloperPrivateAPI() override;

  // Adds a path to the list of allowed unpacked paths for the given
  // `web_contents`. Returns a unique identifier to retry that path. Safe to
  // call multiple times for the same <web_contents, path> pair; each call will
  // return the same identifier.
  UnpackedRetryId AddUnpackedPath(content::WebContents* web_contents,
                                  const base::FilePath& path);

  // Returns the FilePath associated with the given `id` and `web_contents`, if
  // one exists.
  base::FilePath GetUnpackedPath(content::WebContents* web_contents,
                                 const UnpackedRetryId& id) const;

  // Sets the dragged file for the given `web_contents`.
  void SetDraggedFile(content::WebContents* web_contents,
                      const ui::FileInfo& file);

  // Returns the dragged file for the given `web_contents`, if one exists.
  ui::FileInfo GetDraggedFile(content::WebContents* web_contents) const;

  // KeyedService implementation
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  DeveloperPrivateEventRouter* developer_private_event_router() {
    return developer_private_event_router_.get();
  }
  const base::FilePath& last_unpacked_directory() const {
    return last_unpacked_directory_;
  }

 private:
  class WebContentsTracker;

  using IdToPathMap = std::map<UnpackedRetryId, base::FilePath>;
  // Data specific to a given WebContents.
  struct WebContentsData {
    WebContentsData();

    WebContentsData(const WebContentsData&) = delete;
    WebContentsData& operator=(const WebContentsData&) = delete;

    ~WebContentsData();
    WebContentsData(WebContentsData&& other);

    // A set of unpacked paths that we are allowed to load for different
    // WebContents. For security reasons, we don't let JavaScript arbitrarily
    // pass us a path and load the extension at that location; instead, the user
    // has to explicitly select the path through a native dialog first, and then
    // we will allow JavaScript to request we reload that same selected path.
    // Additionally, these are segmented by WebContents; this is primarily to
    // allow collection (removing old paths when the WebContents closes) but has
    // the effect that WebContents A cannot retry a path selected in
    // WebContents B.
    IdToPathMap allowed_unpacked_paths;

    // The last dragged file for the WebContents.
    ui::FileInfo dragged_file;
  };

  friend class BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "DeveloperPrivateAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  void RegisterNotifications();

  const WebContentsData* GetWebContentsData(
      content::WebContents* web_contents) const;
  WebContentsData* GetOrCreateWebContentsData(
      content::WebContents* web_contents);

  raw_ptr<Profile> profile_;

  // Used to start the load `load_extension_dialog_` in the last directory that
  // was loaded.
  base::FilePath last_unpacked_directory_;

  std::map<content::WebContents*, WebContentsData> web_contents_data_;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<DeveloperPrivateEventRouter> developer_private_event_router_;

  base::WeakPtrFactory<DeveloperPrivateAPI> weak_factory_{this};
};

template <>
void BrowserContextKeyedAPIFactory<
    DeveloperPrivateAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
