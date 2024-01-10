// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CONTROLLER_H_
#define ASH_PICKER_PICKER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "base/scoped_observation.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class PickerClient;
class PickerInsertMediaRequest;

// Controls a Picker widget.
class ASH_EXPORT PickerController
    : public PickerViewDelegate,
      public ash::input_method::ImeKeyboard::Observer {
 public:
  PickerController();
  PickerController(const PickerController&) = delete;
  PickerController& operator=(const PickerController&) = delete;
  ~PickerController() override;

  // Whether the provided feature key for Picker can enable the feature.
  static bool IsFeatureKeyMatched();

  // Sets the `client` used by this class and the widget to communicate with the
  // browser. `client` may be set to null, which will close the Widget if it's
  // open. If `client` is not null, then it must remain valid for the lifetime
  // of this class, or until `SetClient` is called with a different client.
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

  // PickerViewDelegate:
  std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) override;
  void LoadAndDecodeGif(const GURL& url, DecodeGifCallback callback) override;
  void GetResultsForCategory(PickerCategory category,
                             SearchResultsCallback callback) override;
  void StartSearch(const std::u16string& query,
                   std::optional<PickerCategory> category,
                   SearchResultsCallback callback) override;
  void InsertResultOnNextFocus(const PickerSearchResult& result) override;
  bool ShouldPaint() override;

  // ash::input_method::ImeKeyboard::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnLayoutChanging(const std::string& layout_name) override {}

 private:
  raw_ptr<PickerClient> client_ = nullptr;
  views::UniqueWidgetPtr widget_;
  bool should_paint_ = false;
  std::unique_ptr<PickerInsertMediaRequest> insert_media_request_;
  base::ScopedObservation<ash::input_method::ImeKeyboard,
                          ash::input_method::ImeKeyboard::Observer>
      observation_{this};
};

}  // namespace ash

#endif
