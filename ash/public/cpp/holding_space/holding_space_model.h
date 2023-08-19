// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_id.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

namespace cros_styles {
enum class ColorName;
}  // namespace cros_styles

namespace ash {

class HoldingSpaceModelObserver;

// The data model for the temporary holding space UI. It contains the list of
// items that should be shown in the temporary holding space UI - each item will
// represent a piece of data added to the holding space by the user (for
// example, text, URLs, or images).
// The main goal of the class is to provide UI implementation agnostic
// information about items added to the holding space, and to provide an
// interface to propagate holding space changes between ash and Chrome.
class ASH_PUBLIC_EXPORT HoldingSpaceModel {
 public:
  using ItemList = std::vector<std::unique_ptr<HoldingSpaceItem>>;

  // A class which performs an atomic update of a single holding space item on
  // destruction, notifying model observers of the event if a change in state
  // did in fact occur.
  class ScopedItemUpdate {
   public:
    ScopedItemUpdate(const ScopedItemUpdate&) = delete;
    ScopedItemUpdate& operator=(const ScopedItemUpdate&) = delete;
    ~ScopedItemUpdate();

    // Sets the accessible name that should be used for the item and returns a
    // reference to `this`.
    ScopedItemUpdate& SetAccessibleName(
        const absl::optional<std::u16string>& accessible_name);

    // TODO(http://b/288471183): Remove file path and file system URL.
    // Sets the backing file for the item and returns a reference to `this`.
    ScopedItemUpdate& SetBackingFile(const HoldingSpaceFile& file,
                                     const base::FilePath& file_path,
                                     const GURL& file_system_url);

    // Sets the commands for an in-progress item which are shown in the item's
    // context menu and possibly, in the case of cancel/pause/resume, as
    // primary/secondary actions on the item view itself.
    ScopedItemUpdate& SetInProgressCommands(
        std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands);

    // Sets whether the image for the item should be forcibly invalidated and
    // returns a reference to `this`.
    ScopedItemUpdate& SetInvalidateImage(bool invalidate_image);

    // Sets the `progress` of the item and returns a reference to `this`.
    // NOTE: Only in-progress holding space items can be progressed.
    ScopedItemUpdate& SetProgress(const HoldingSpaceProgress& progress);

    // Sets the secondary text that should be shown for the item and returns a
    // reference to `this`.
    ScopedItemUpdate& SetSecondaryText(
        const absl::optional<std::u16string>& secondary_text);

    // Sets the color id for the secondary text that should be shown for the
    // item and returns a reference to `this`.
    ScopedItemUpdate& SetSecondaryTextColorId(
        const absl::optional<ui::ColorId>& secondary_text_color);

    // Sets the text that should be shown for the item and returns a reference
    // to `this`. If absent, the lossy display name of the backing file will be
    // used.
    ScopedItemUpdate& SetText(const absl::optional<std::u16string>& text);

   private:
    friend class HoldingSpaceModel;
    ScopedItemUpdate(HoldingSpaceModel* model, HoldingSpaceItem* item);

    const raw_ptr<HoldingSpaceModel, ExperimentalAsh> model_;
    const raw_ptr<HoldingSpaceItem, ExperimentalAsh> item_;

    absl::optional<absl::optional<std::u16string>> accessible_name_;
    absl::optional<HoldingSpaceFile> file_;
    absl::optional<base::FilePath> file_path_;
    absl::optional<GURL> file_system_url_;
    absl::optional<std::vector<HoldingSpaceItem::InProgressCommand>>
        in_progress_commands_;
    absl::optional<HoldingSpaceProgress> progress_;
    absl::optional<absl::optional<std::u16string>> secondary_text_;
    absl::optional<absl::optional<ui::ColorId>> secondary_text_color_id_;
    absl::optional<absl::optional<std::u16string>> text_;
    bool invalidate_image_ = false;
  };

  HoldingSpaceModel();
  HoldingSpaceModel(const HoldingSpaceModel& other) = delete;
  HoldingSpaceModel& operator=(const HoldingSpaceModel& other) = delete;
  ~HoldingSpaceModel();

  // Adds a single holding space item to the model.
  void AddItem(std::unique_ptr<HoldingSpaceItem> item);

  // Adds multiple holding space items to the model.
  void AddItems(std::vector<std::unique_ptr<HoldingSpaceItem>> items);

  // Removes a single holding space item from the model.
  void RemoveItem(const std::string& id);

  // Removes multiple holding space items from the model.
  void RemoveItems(const std::set<std::string>& ids);

  // Similar to `RemoveItem()` but returns the unique pointer to the removed
  // item. If the specified item does not exist in the model, returns `nullptr`.
  std::unique_ptr<HoldingSpaceItem> TakeItem(const std::string& id);

  // TODO(http://b/288471183): Remove file system URL.
  // Fully initializes a partially initialized holding space item using the
  // provided `file` and `file_system_url`. The item will be removed if
  // `file_system_url` is empty.
  void InitializeOrRemoveItem(const std::string& id,
                              const HoldingSpaceFile& file,
                              const GURL& file_system_url);

  // Returns an object which, upon its destruction, performs an atomic update to
  // the holding space item associated with the specified `id`.
  std::unique_ptr<ScopedItemUpdate> UpdateItem(const std::string& id);

  // Removes all holding space items from the model for which the specified
  // `predicate` returns true. Returns the unique pointers to the items removed
  // from the model.
  using Predicate = base::RepeatingCallback<bool(const HoldingSpaceItem*)>;
  std::vector<std::unique_ptr<HoldingSpaceItem>> RemoveIf(Predicate predicate);

  // Invalidates image representations for items for which the specified
  // `predicate` returns true.
  void InvalidateItemImageIf(Predicate predicate);

  // Removes all the items from the model.
  void RemoveAll();

  // Gets a single holding space item.
  // Returns nullptr if the item does not exist in the model.
  const HoldingSpaceItem* GetItem(const std::string& id) const;

  // Gets a single holding space item with the specified `type` backed by the
  // specified `file_path`. Returns `nullptr` if the item does not exist in the
  // model.
  const HoldingSpaceItem* GetItem(HoldingSpaceItem::Type type,
                                  const base::FilePath& file_path) const;

  // Returns whether or not there exists a holding space item of the specified
  // `type` backed by the specified `file_path`.
  bool ContainsItem(HoldingSpaceItem::Type type,
                    const base::FilePath& file_path) const;

  // Returns `true` if the model contains any initialized items of the specified
  // `type`, `false` otherwise.
  bool ContainsInitializedItemOfType(HoldingSpaceItem::Type type) const;

  const ItemList& items() const { return items_; }

  void AddObserver(HoldingSpaceModelObserver* observer);
  void RemoveObserver(HoldingSpaceModelObserver* observer);

 private:
  // The list of items added to the model in the order they have been added to
  // the model.
  ItemList items_;

  // Caches the count of initialized items in the model for each holding space
  // item type. Used to quickly look up whether the model contains any
  // initialized items of a given type.
  std::map<HoldingSpaceItem::Type, size_t> initialized_item_counts_by_type_;

  base::ObserverList<HoldingSpaceModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
