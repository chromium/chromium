// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace cros_styles {
enum class ColorName;
}  // namespace cros_styles

namespace ash {

class HoldingSpaceImage;

// Contains data needed to display a single item in the holding space UI.
class ASH_PUBLIC_EXPORT HoldingSpaceItem {
 public:
  // Models a command for an in-progress item which is shown in the item's
  // context menu and possibly, in the case of cancel/pause/resume, as primary/
  // secondary actions on the item's view itself.
  struct InProgressCommand {
   public:
    using Handler =
        base::RepeatingCallback<void(const HoldingSpaceItem* item,
                                     HoldingSpaceCommandId command_id)>;

    InProgressCommand(HoldingSpaceCommandId command_id,
                      int label_id,
                      const gfx::VectorIcon* icon,
                      Handler handler);

    InProgressCommand(const InProgressCommand& other);

    InProgressCommand& operator=(const InProgressCommand& other);

    ~InProgressCommand();

    bool operator==(const InProgressCommand& other) const;

    // The identifier for the command.
    HoldingSpaceCommandId command_id;

    // The identifier for the label to be displayed for the command.
    int label_id;

    // The icon to be displayed for the command.
    raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon;

    // The handler to be invoked to perform command execution.
    Handler handler;
  };

  // Items types supported by the holding space.
  // NOTE: These values are recorded in histograms and persisted in preferences
  // so append new values to the end and do not change the meaning of existing
  // values.
  enum class Type {
    kPinnedFile = 0,
    kScreenshot = 1,
    kDownload = 2,
    kNearbyShare = 3,
    kScreenRecording = 4,
    kArcDownload = 5,
    kPrintedPdf = 6,
    kDiagnosticsLog = 7,
    kLacrosDownload = 8,
    kScan = 9,
    kPhoneHubCameraRoll = 10,
    kDriveSuggestion = 11,
    kLocalSuggestion = 12,
    kScreenRecordingGif = 13,
    kCameraAppPhoto = 14,
    kCameraAppScanJpg = 15,
    kCameraAppScanPdf = 16,
    kCameraAppVideoGif = 17,
    kCameraAppVideoMp4 = 18,
    kMaxValue = kCameraAppVideoMp4,
  };

  HoldingSpaceItem(const HoldingSpaceItem&) = delete;
  HoldingSpaceItem operator=(const HoldingSpaceItem&) = delete;
  ~HoldingSpaceItem();

  bool operator==(const HoldingSpaceItem& rhs) const;

  // Returns an image for a given type and file path.
  using ImageResolver = base::OnceCallback<
      std::unique_ptr<HoldingSpaceImage>(Type, const base::FilePath&)>;

  // Creates a HoldingSpaceItem that's backed by a file system URL.
  // NOTE: `file_system_url` is expected to be non-empty.
  static std::unique_ptr<HoldingSpaceItem> CreateFileBackedItem(
      Type type,
      const base::FilePath& file_path,
      const GURL& file_system_url,
      ImageResolver image_resolver);

  // Creates a HoldingSpaceItem that's backed by a file system URL.
  // NOTE: `file_system_url` is expected to be non-empty.
  static std::unique_ptr<HoldingSpaceItem> CreateFileBackedItem(
      Type type,
      const base::FilePath& file_path,
      const GURL& file_system_url,
      const HoldingSpaceProgress& progress,
      ImageResolver image_resolver);

  // Returns `true` if `type` is a Camera app type, `false` otherwise.
  static bool IsCameraAppType(HoldingSpaceItem::Type type);

  // Returns `true` if `type` is a download type, `false` otherwise.
  static bool IsDownloadType(HoldingSpaceItem::Type type);

  // Returns `true` if `type` is a screen capture type, `false` otherwise.
  static bool IsScreenCaptureType(HoldingSpaceItem::Type type);

  // Returns `true` if `type` is a suggestion type, `false` otherwise.
  static bool IsSuggestionType(HoldingSpaceItem::Type type);

  // Deserializes from `base::Value::Dict` to `HoldingSpaceItem`.
  // This creates a partially initialized item with an empty file system URL.
  // The item should be fully initialized using `Initialize()`.
  static std::unique_ptr<HoldingSpaceItem> Deserialize(
      const base::Value::Dict& dict,
      ImageResolver image_resolver);

  // Deserializes `id_` from a serialized `HoldingSpaceItem`.
  static const std::string& DeserializeId(const base::Value::Dict& dict);

  // Deserializes `file_path_` from a serialized `HoldingSpaceItem`.
  static base::FilePath DeserializeFilePath(const base::Value::Dict& dict);

  // Deserializes `type_` from a serialized `HoldingSpaceItem`.
  static Type DeserializeType(const base::Value::Dict& dict);

  // Serializes from `HoldingSpaceItem` to `base::Value::Dict`.
  base::Value::Dict Serialize() const;

  // Adds `callback` to be notified when `this` gets deleted.
  base::CallbackListSubscription AddDeletionCallback(
      base::RepeatingClosureList::CallbackType callback) const;

  // Indicates whether the item has been initialized. This will be false for
  // items created using `Deserialize()` for which `Initialize()` has not yet
  // been called. Non-initialized items should not be shown in holding space UI.
  bool IsInitialized() const;

  // Used to fully initialize partially initialized items created by
  // `Deserialize()`.
  void Initialize(const GURL& file_system_url);

  // Sets the file backing the item to `file_path` and `file_system_url`,
  // returning `true` if a change occurred or `false` to indicate no-op.
  bool SetBackingFile(const base::FilePath& file_path,
                      const GURL& file_system_url);

  // Returns `text_`, falling back to the lossy display name of the item's
  // backing file if absent.
  std::u16string GetText() const;

  // Sets the text that should be shown for the item, returning `true` if a
  // change occurred or `false` to indicate no-op. If absent, the lossy display
  // name of the item's backing file will be used.
  bool SetText(const absl::optional<std::u16string>& text);

  // Sets the secondary text that should be shown for the item, returning `true`
  // if a change occurred or `false` to indicate no-op.
  bool SetSecondaryText(const absl::optional<std::u16string>& secondary_text);

  // Sets the color id for the secondary text that should be shown for the item,
  // returning `true` if a change occurred or `false` to indicate no-op. If
  // `absl::nullopt` is provided, secondary text color will fallback to default.
  bool SetSecondaryTextColorId(
      const absl::optional<ui::ColorId>& secondary_text_color_id);

  // Returns `accessible_name_`, falling back to a concatenation of primary
  // and secondary text if absent.
  std::u16string GetAccessibleName() const;

  // Sets the accessible name that should be used for the item, returning `true`
  // if a change occurred or `false` to indicate no-op. Note that if the
  // accessible name is absent, `GetAccessibleName()` will fallback to a
  // concatenation of primary and secondary text.
  bool SetAccessibleName(const absl::optional<std::u16string>& accessible_name);

  // Sets the commands for an in-progress item which are shown in the item's
  // context menu and possibly, in the case of cancel/pause/resume, as primary/
  // secondary actions on the item view itself.
  bool SetInProgressCommands(
      std::vector<InProgressCommand> in_progress_commands);

  // Sets the `progress_` of the item, returning `true` if a change occurred or
  // `false` to indicate no-op.
  // NOTE: Progress can only be updated for in progress items.
  bool SetProgress(const HoldingSpaceProgress& progress);

  // Invalidates the current holding space image, so fresh image representations
  // are loaded when the image is next needed.
  void InvalidateImage();

  const std::string& id() const { return id_; }

  Type type() const { return type_; }

  const absl::optional<std::u16string>& secondary_text() const {
    return secondary_text_;
  }

  const absl::optional<ui::ColorId>& secondary_text_color_id() const {
    return secondary_text_color_id_;
  }

  const HoldingSpaceImage& image() const { return *image_; }

  const base::FilePath& file_path() const { return file_path_; }

  const GURL& file_system_url() const { return file_system_url_; }

  const HoldingSpaceProgress& progress() const { return progress_; }

  const std::vector<InProgressCommand>& in_progress_commands() const {
    return in_progress_commands_;
  }

  HoldingSpaceImage& image_for_testing() { return *image_; }

 private:
  // Constructor for file backed items.
  HoldingSpaceItem(Type type,
                   const std::string& id,
                   const base::FilePath& file_path,
                   const GURL& file_system_url,
                   std::unique_ptr<HoldingSpaceImage> image,
                   const HoldingSpaceProgress& progress);

  const Type type_;

  // The holding space item ID assigned to the item.
  std::string id_;

  // The file path by which the item is backed.
  base::FilePath file_path_;

  // The file system URL of the file that backs the item.
  GURL file_system_url_;

  // If set, the text that should be shown for the item.
  absl::optional<std::u16string> text_;

  // If set, the secondary text that should be shown for the item.
  absl::optional<std::u16string> secondary_text_;

  // If set, the color resolved from the color id for the secondary text that
  // should be shown for the item.
  absl::optional<ui::ColorId> secondary_text_color_id_;

  // If set, the accessible name that should be used for the item.
  absl::optional<std::u16string> accessible_name_;

  // The image representation of the item.
  std::unique_ptr<HoldingSpaceImage> image_;

  // The progress of the item.
  HoldingSpaceProgress progress_;

  // The commands for an in-progress item which are shown in the item's context
  // menu and possibly, in the case of cancel/pause/resume, as primary/secondary
  // actions on the item's view itself.
  std::vector<InProgressCommand> in_progress_commands_;

  // Mutable to allow const access from `AddDeletionCallback()`.
  mutable base::RepeatingClosureList deletion_callback_list_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
