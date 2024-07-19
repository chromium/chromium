// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CONTROLLER_H_
#define ASH_PICKER_PICKER_CONTROLLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/metrics/picker_feature_usage_metrics.h"
#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/picker_asset_fetcher_impl_delegate.h"
#include "ash/picker/views/picker_feature_tour.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class PickerAssetFetcher;
class PickerCapsLockStateView;
class PickerClient;
class PickerEmojiHistoryModel;
class PickerEmojiSuggester;
class PickerInsertMediaRequest;
class PickerModel;
class PickerPasteRequest;
class PickerSearchController;
class PickerSearchResult;
class PickerSuggestionsController;

// Controls a Picker widget.
class ASH_EXPORT PickerController : public PickerViewDelegate,
                                    public views::WidgetObserver,
                                    public PickerAssetFetcherImplDelegate {
 public:
  PickerController();
  PickerController(const PickerController&) = delete;
  PickerController& operator=(const PickerController&) = delete;
  ~PickerController() override;

  // Whether the provided feature key for Picker can enable the feature.
  static bool IsFeatureKeyMatched();

  // Time from when the insert is issued and when we give up inserting.
  static constexpr base::TimeDelta kInsertMediaTimeout = base::Seconds(2);

  // Time from when a search starts to when the first set of results are
  // published.
  static constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(200);

  // Sets the `client` used by this class and the widget to communicate with the
  // browser. `client` may be set to null, which will close the Widget if it's
  // open, and may call "stop search" methods on the PREVIOUS client.
  // If `client` is not null, then it must remain valid for the lifetime of this
  // class, or until AFTER `SetClient` is called with a different client.
  // Caution: If `client` outlives this class, the client should avoid calling
  // this method on a destructed class instance to avoid a use after free.
  void SetClient(PickerClient* client);

  // Toggles the visibility of the Picker widget.
  // This must only be called after `SetClient` is called with a valid client.
  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // Widget to be toggled. For example, if the feature was triggered by a mouse
  // click, then it should be the timestamp of the click. By default, the
  // timestamp is the time this function is called.
  void ToggleWidget(
      base::TimeTicks trigger_event_timestamp = base::TimeTicks::Now());

  // Returns the Picker widget for tests.
  views::Widget* widget_for_testing() { return widget_.get(); }
  // Returns the CapsLock state view for tests.
  PickerCapsLockStateView* caps_lock_state_view_for_testing() {
    return caps_lock_state_view_.get();
  }
  PickerFeatureTour& feature_tour_for_testing() { return feature_tour_; }

  // PickerViewDelegate:
  std::vector<PickerCategory> GetAvailableCategories() override;
  void GetZeroStateSuggestedResults(SuggestedResultsCallback callback) override;
  void GetResultsForCategory(PickerCategory category,
                             SearchResultsCallback callback) override;
  void StartSearch(std::u16string_view query,
                   std::optional<PickerCategory> category,
                   SearchResultsCallback callback) override;
  void StopSearch() override;
  void StartEmojiSearch(std::u16string_view,
                        EmojiSearchResultsCallback callback) override;
  void InsertResultOnNextFocus(const PickerSearchResult& result) override;
  void OpenResult(const PickerSearchResult& result) override;
  void ShowEmojiPicker(ui::EmojiPickerCategory category,
                       std::u16string_view query) override;
  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text) override;
  PickerAssetFetcher* GetAssetFetcher() override;
  PickerSessionMetrics& GetSessionMetrics() override;
  PickerActionType GetActionForResult(
      const PickerSearchResult& result) override;
  std::vector<PickerSearchResult> GetSuggestedEmoji() override;
  bool IsGifsEnabled() override;

  // views:WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // PickerAssetFetcherImplDelegate:
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;

  // Disables the feature key checking. Only works in tests.
  static void DisableFeatureKeyCheckForTesting();

 private:
  void ShowWidget(base::TimeTicks trigger_event_timestamp);
  void CloseWidget();
  void OnFeatureTourCompleted();
  void CloseCapsLockStateView();

  PickerFeatureTour feature_tour_;
  std::unique_ptr<PickerModel> model_;
  std::unique_ptr<PickerEmojiHistoryModel> emoji_history_model_;
  std::unique_ptr<PickerEmojiSuggester> emoji_suggester_;
  views::UniqueWidgetPtr widget_;
  std::unique_ptr<PickerAssetFetcher> asset_fetcher_;
  std::unique_ptr<PickerInsertMediaRequest> insert_media_request_;
  std::unique_ptr<PickerPasteRequest> paste_request_;
  std::unique_ptr<PickerSuggestionsController> suggestions_controller_;
  std::unique_ptr<PickerSearchController> search_controller_;

  raw_ptr<PickerClient> client_ = nullptr;
  raw_ptr<PickerCapsLockStateView> caps_lock_state_view_ = nullptr;

  base::OnceCallback<void(std::optional<std::string> preset_query_id,
                          std::optional<std::string> freeform_text)>
      show_editor_callback_;

  // Periodically records usage metrics based on the Standard Feature Usage
  // Logging (SFUL) framework.
  PickerFeatureUsageMetrics feature_usage_metrics_;

  // Records metrics related to a session.
  std::unique_ptr<PickerSessionMetrics> session_metrics_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // Closes CapsLock state view after some time.
  base::OneShotTimer caps_lock_state_view_close_timer_;

  base::WeakPtrFactory<PickerController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_CONTROLLER_H_
