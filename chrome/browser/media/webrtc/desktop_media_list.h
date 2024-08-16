// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/gfx/image/image_skia.h"

class DesktopMediaListObserver;

namespace content {
class WebContents;
}

// DesktopMediaList provides the list of desktop media source (screens, windows,
// tabs), and their thumbnails, to the desktop media picker dialog. It
// transparently updates the list in the background, and notifies the desktop
// media picker when something changes.
//
// TODO(crbug.com/40637301): Consider renaming this class.
class DesktopMediaList {
 public:
  // Reflects content::DesktopMediaID::Type, but can decorate it with additional
  // information. For example, we can specify *current* screen/window/type while
  // keeping all other code that uses content::DesktopMediaID::Type unchanged.
  enum class Type {
    kNone,         // TYPE_NONE
    kScreen,       // TYPE_SCREEN
    kWindow,       // TYPE_WINDOW
    kWebContents,  // TYPE_WEB_CONTENTS
    kCurrentTab,   // TYPE_WEB_CONTENTS of the current tab.
  };

  // A WebContents filter can be applied to DesktopMediaList::Type::kWebContents
  // MediaList object in order to provide a way to filter out any WebContents
  // that shouldn't be included.
  using WebContentsFilter =
      base::RepeatingCallback<bool(content::WebContents*)>;

  // Wraps a given filter to produce a new filter that excludes a given
  // WebContents, but is otherwise identical to the original filter.
  //
  // Note: |excluded_web_contents| will internally be converted to a WeakPtr
  // in order to make posting the filter safe. If that weak pointer expires,
  // the exclusion also expires. This is safe even when the capturer/capturee
  // are the same, because capturing itself will be rejected in that case.
  static WebContentsFilter ExcludeWebContents(
      WebContentsFilter filter,
      content::WebContents* excluded_web_contents);

  // Struct used to represent each entry in the list.
  struct Source {
    Source();
    Source(const Source& other_source);
    ~Source();

    // Id of the source.
    content::DesktopMediaID id;

    // Name of the source that should be shown to the user.
    std::u16string name;

    // The thumbnail for the source.
    gfx::ImageSkia thumbnail;

    // A preview for this source, used when both a thumbnail and preview are
    // used. Currently only the case in the tab_desktop_media_list.
    gfx::ImageSkia preview;
  };

  using UpdateCallback = base::OnceClosure;

  virtual ~DesktopMediaList() {}

  // Sets time interval between updates. By default list of sources and their
  // thumbnail are updated once per second. If called after StartUpdating() then
  // it will take effect only after the next update.
  virtual void SetUpdatePeriod(base::TimeDelta period) = 0;

  // Sets size to which the thumbnails should be scaled. If called after
  // StartUpdating() then some thumbnails may be still scaled to the old size
  // until they are updated.
  virtual void SetThumbnailSize(const gfx::Size& thumbnail_size) = 0;

  // Sets ID of the hosting desktop picker dialog. The window with this ID will
  // be filtered out from the list of sources.
  virtual void SetViewDialogWindowId(content::DesktopMediaID dialog_id) = 0;

  // Starts updating the model. The model is initially empty, so OnSourceAdded()
  // notifications will be generated for each existing source as it is
  // enumerated. After the initial enumeration the model will be refreshed based
  // on the update period, and notifications generated only for changes in the
  // model.
  virtual void StartUpdating(DesktopMediaListObserver* observer) = 0;

  // Updates the model and calls |callback| when all currently existing sources
  // have been found, passing |this| as the argument.  In most respects, this is
  // a simplified version of StartUpdating().  This method should only be called
  // once per DesktopMediaList instance.  It should not be called after
  // StartUpdating(), and StartUpdating() should not be called until |callback|
  // has been called.
  virtual void Update(UpdateCallback callback) = 0;

  virtual int GetSourceCount() const = 0;
  virtual const Source& GetSource(int index) const = 0;

  virtual Type GetMediaListType() const = 0;

  // Set or clear the id of a single source which needs a preview image
  // generating in addition to its thumbnail.
  virtual void SetPreviewedSource(
      const std::optional<content::DesktopMediaID>& id) = 0;

  // Returns true if this DesktopMediaList wraps some other object (usually a
  // DesktopCapturer), that takes responsibility for showing its own source
  // list where the user will likely make their selection. When true, there will
  // only be one source listed which will represent the selection made in the
  // delegated source list.
  // Returns false if this DesktopMediaList needs UI created for it to show its
  // source list.
  virtual bool IsSourceListDelegated() const = 0;

  // May only be called if |IsSourceListDelegated| returns true. When called,
  // clears any selection from the delegated source list and will cause the
  // delegated source list to reappear if it is focused.
  virtual void ClearDelegatedSourceListSelection() = 0;

  // Notifies the list that it is now focused. This is especially important
  // when IsSourceDelegated() returns true, as it helps to notify the delegated
  // source list when it should be visible.
  virtual void FocusList() = 0;

  // Notifies the list that it is no longer focused. This is especially
  // important when IsSourceDelegated() returns true, as it helps to notify the
  // delegated source list when it should be hidden.
  virtual void HideList() = 0;

  // Show the delegated source list. This is intended to be used for delegated
  // source lists that need to be displayed independently from when the
  // DesktopMediaList gains focus.
  virtual void ShowDelegatedList() = 0;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_LIST_H_
