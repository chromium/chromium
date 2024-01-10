// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_category.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/image_util.h"

class GURL;

namespace ash {

class PickerSearchResult;
class PickerSearchResults;

// Delegate for `PickerView`.
class ASH_EXPORT PickerViewDelegate {
 public:
  using SearchResultsCallback =
      base::RepeatingCallback<void(const PickerSearchResults& results)>;
  // TODO: b/316936723 - Pass `frames` by reference to avoid a copy.
  using DecodeGifCallback =
      base::OnceCallback<void(std::vector<image_util::AnimationFrame> frames)>;

  virtual ~PickerViewDelegate() {}

  virtual std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) = 0;

  // Loads and decodes a gif from `url`. If successful, the decoded gif frames
  // will be returned via `callback`. Otherwise, `callback` is run with an empty
  // vector of frames.
  virtual void LoadAndDecodeGif(const GURL& url,
                                DecodeGifCallback callback) = 0;

  // Gets initially suggested results for category. Results will be returned via
  // `callback`, which may be called multiples times to update the results.
  virtual void GetResultsForCategory(PickerCategory category,
                                     SearchResultsCallback callback) = 0;

  // Starts a search for `query`. Results will be returned via `callback`,
  // which may be called multiples times to update the results.
  virtual void StartSearch(const std::u16string& query,
                           std::optional<PickerCategory> category,
                           SearchResultsCallback callback) = 0;

  // Inserts `result` into the next focused input field.
  // If there's no focus event within some timeout after the widget is closed,
  // the result is dropped silently.
  virtual void InsertResultOnNextFocus(const PickerSearchResult& result) = 0;

  // Whether the view should paint. Certain test scenarios do not need
  // painting, so it is better to skip painting.
  virtual bool ShouldPaint() = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_
