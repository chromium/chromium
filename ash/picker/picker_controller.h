// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CONTROLLER_H_
#define ASH_PICKER_PICKER_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/metrics/picker_feature_usage_metrics.h"
#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/model/picker_emoji_history_model.h"
#include "ash/picker/model/picker_emoji_suggester.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/picker_asset_fetcher_impl_delegate.h"
#include "ash/picker/picker_caps_lock_bubble_controller.h"
#include "ash/picker/picker_insert_media_request.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/picker_suggestions_controller.h"
#include "ash/picker/picker_web_paste_target.h"
#include "ash/picker/search/picker_search_controller.h"
#include "ash/picker/views/picker_feature_tour.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PrefService;

namespace input_method {
class ImeKeyboard;
}

namespace ui {
class TextInputClient;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class PickerAssetFetcher;
class PickerClient;
class PickerModel;
class PickerPasteRequest;
class PickerActionOnNextFocusRequest;

// Controls a Picker widget.
class ASH_EXPORT PickerController : public PickerViewDelegate,
                                    public views::ViewObserver,
                                    public PickerAssetFetcherImplDelegate {
 public:
  PickerController();
  PickerController(const PickerController&) = delete;
  PickerController& operator=(const PickerController&) = delete;
  ~PickerController() override;

  // Maximum time to wait for focus to be regained after completing the feature
  // tour. If this timeout is reached, we stop waiting for focus and show the
  // Picker widget regardless of the focus state.
  static constexpr base::TimeDelta kShowWidgetPostFeatureTourTimeout =
      base::Seconds(2);

  // Time from when the insert is issued and when we give up inserting.
  static constexpr base::TimeDelta kInsertMediaTimeout = base::Seconds(2);

  // Time from when a search starts to when the first set of results are
  // published.
  static constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(200);

  // Whether the feature is currently enabled or not based on the secret key and
  // other factors.
  bool IsFeatureEnabled();

  // Sets the `client` used by this class and the widget to communicate with the
  // browser. `client` may be set to null, which will close the Widget if it's
  // open, and may call "stop search" methods on the PREVIOUS client.
  // If `client` is not null, then it must remain valid for the lifetime of this
  // class, or until AFTER `SetClient` is called with a different client.
  // Caution: If `client` outlives this class, the client should avoid calling
  // this method on a destructed class instance to avoid a use after free.
  void SetClient(PickerClient* client);

  // This should be run when the Prefs from the client is ready.
  void OnClientPrefsSet(PrefService* prefs);

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
  PickerFeatureTour& feature_tour_for_testing() { return feature_tour_; }
  PickerCapsLockBubbleController& caps_lock_bubble_controller_for_testing() {
    return caps_lock_bubble_controller_;
  }

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
  void CloseWidgetThenInsertResultOnNextFocus(
      const PickerSearchResult& result) override;
  void OpenResult(const PickerSearchResult& result) override;
  void ShowEmojiPicker(ui::EmojiPickerCategory category,
                       std::u16string_view query) override;
  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text) override;
  void ShowLobster(std::optional<std::string> freeform_text) override;
  PickerAssetFetcher* GetAssetFetcher() override;
  PickerSessionMetrics& GetSessionMetrics() override;
  PickerActionType GetActionForResult(
      const PickerSearchResult& result) override;
  std::vector<PickerEmojiResult> GetSuggestedEmoji() override;
  bool IsGifsEnabled() override;
  PickerModeType GetMode() override;
  PickerCapsLockPosition GetCapsLockPosition() override;

  // views:ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  // PickerAssetFetcherImplDelegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;

  // Disables the feature tour. Only works in tests.
  static void DisableFeatureTourForTesting();

 private:
  // Trigger source for showing the Picker widget. This is used to determine
  // how the widget should be shown on the screen.
  enum class WidgetTriggerSource {
    // The user triggered Picker as part of their usual user flow, e.g. toggled
    // Picker with a key press.
    kDefault,
    // The user triggered Picker by completing the feature tour.
    kFeatureTour,
  };

  // Active Picker session tied to the lifetime of the PickerWidget.
  struct Session {
    PickerModel model;
    PickerEmojiHistoryModel emoji_history_model;
    PickerEmojiSuggester emoji_suggester;
    PickerSessionMetrics session_metrics;
    // Periodically records usage metrics based on the Standard Feature Usage
    // Logging (SFUL) framework.
    PickerFeatureUsageMetrics feature_usage_metrics;

    Session(PrefService* prefs,
            ui::TextInputClient* focused_client,
            input_method::ImeKeyboard* ime_keyboard,
            PickerModel::EditorStatus editor_status,
            PickerModel::LobsterStatus lobster_status,
            PickerEmojiSuggester::GetNameCallback get_name);
    ~Session();
  };

  void ShowWidget(base::TimeTicks trigger_event_timestamp,
                  WidgetTriggerSource trigger_source);
  void CloseWidget();
  void ShowWidgetPostFeatureTour();
  void InsertResultOnNextFocus(const PickerSearchResult& result);
  void OnInsertCompleted(const PickerRichMedia& media,
                         PickerInsertMediaRequest::Result result);
  PrefService* GetPrefs();

  std::optional<PickerWebPasteTarget> GetWebPasteTarget();

  PickerFeatureTour feature_tour_;
  PickerCapsLockBubbleController caps_lock_bubble_controller_;
  std::unique_ptr<Session> session_;
  views::UniqueWidgetPtr widget_;
  std::unique_ptr<PickerAssetFetcher> asset_fetcher_;
  std::unique_ptr<PickerInsertMediaRequest> insert_media_request_;
  std::unique_ptr<PickerPasteRequest> paste_request_;
  std::unique_ptr<PickerActionOnNextFocusRequest> caps_lock_request_;
  PickerSuggestionsController suggestions_controller_;
  PickerSearchController search_controller_;

  raw_ptr<PickerClient> client_ = nullptr;

  base::OnceCallback<void(std::optional<std::string> preset_query_id,
                          std::optional<std::string> freeform_text)>
      show_editor_callback_;

  base::OnceCallback<void(std::optional<std::string> freeform_text)>
      show_lobster_callback_;

  // Timer used to delay closing the Widget for accessibility.
  base::OneShotTimer close_widget_delay_timer_;

  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};

  base::WeakPtrFactory<PickerController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_CONTROLLER_H_
