// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_RESULT_SELECTION_CONTROLLER_H_
#define ASH_APP_LIST_VIEWS_RESULT_SELECTION_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class SearchResultContainerView;

// This alias is intended to clarify the intended use of this class within the
// context of this controller.
using ResultSelectionModel =
    std::vector<raw_ptr<SearchResultContainerView, VectorExperimental>>;

// Stores and organizes the details for the 'coordinates' of the selected
// result. This includes all information to determine exactly where a result is,
// including both inter- and intra-container details, along with the traversal
// direction for the container.
struct ASH_EXPORT ResultLocationDetails {
  ResultLocationDetails();
  ResultLocationDetails(int container_index,
                        int container_count,
                        int result_index,
                        int result_count,
                        bool container_is_horizontal);

  bool operator==(const ResultLocationDetails& other) const;
  bool operator!=(const ResultLocationDetails& other) const;

  // True if the result is the first(0th) in its container
  bool is_first_result() const { return result_index == 0; }

  // True if the result is the last in its container
  bool is_last_result() const { return result_index == (result_count - 1); }

  // Index of the container within the overall nest of containers.
  int container_index = 0;

  // Number of containers to traverse among.
  int container_count = 0;

  // Index of the result within the list of results inside a container.
  int result_index = 0;

  // Number of results within the current container.
  int result_count = 0;

  // Whether the container is horizontally traversable.
  bool container_is_horizontal = false;
};

// A controller class to manage result selection across containers.
class ASH_EXPORT ResultSelectionController {
 public:
  enum class MoveResult {
    // The selection has not changed (excluding the case covered by
    // kSelectionCycleRejected).
    kNone,

    // The selection has not changed because the selection would cycle.
    kSelectionCycleBeforeFirstResult,
    kSelectionCycleAfterLastResult,

    // The currently selected result has changed.
    //
    // Note: As long as the selected result remains the same, the result action
    // changes will be reported as kNone, mainly because the code that uses
    // MoveSelection() treats them the same.
    kResultChanged,
  };

  ResultSelectionController(
      const ResultSelectionModel* result_container_views,
      const base::RepeatingClosure& selection_change_callback);

  ResultSelectionController(const ResultSelectionController&) = delete;
  ResultSelectionController& operator=(const ResultSelectionController&) =
      delete;

  ~ResultSelectionController();

  // Returns the currently selected result.
  // Note that the return view might contain null result if results are
  // currently being updated.
  // As long as |block_selection_changes_| gets set while results are changing,
  // it should be safe to assume the result is not null after call to
  // MoveSelection() changes the selected result.
  SearchResultBaseView* selected_result() { return selected_result_; }

  // Returns the |ResultLocationDetails| object for the |selected_result_|.
  ResultLocationDetails* selected_location_details() {
    return selected_location_details_.get();
  }

  // Returns whether `selected_result_` locates at the first available location.
  // Returns false if there is no selected result.
  bool IsSelectedResultAtFirstAvailableLocation();

  // Calls |SetSelection| using the result of |GetNextResultLocation|.
  MoveResult MoveSelection(const ui::KeyEvent& event);

  // Resets the selection to the first result.
  // |key_event| - The key event that triggered reselect, if any. Used to
  //     determine whether selection should start at the last element.
  // |default_selection| - True if it resets the first result as default
  //     selection.
  void ResetSelection(const ui::KeyEvent* key_event, bool default_selection);

  // Clears the |selected_result_|, |selected_location_details_|.
  void ClearSelection();

  // Used to block selection changes while async search result updates are in
  // flight, i.e. while the result views might point to obsolete null results.
  // Should be set for a short time, and setting this to false should generally
  // be followed by ResetSelection().
  void set_block_selection_changes(bool block_selection_changes) {
    block_selection_changes_ = block_selection_changes;
  }

 private:
  // Calls |GetNextResultLocationForLocation| using |selected_location_details_|
  // as the location
  MoveResult GetNextResultLocation(const ui::KeyEvent& event,
                                   ResultLocationDetails* next_location);

  // Logic for next is separated for modular use. You can ask for the "next"
  // location to be generated using any starting location/event combination.
  MoveResult GetNextResultLocationForLocation(
      const ui::KeyEvent& event,
      const ResultLocationDetails& location,
      ResultLocationDetails* next_location);

  // Sets the current selection to the provided |location|.
  void SetSelection(const ResultLocationDetails& location,
                    bool reverse_tab_order);

  // Returns the location for the first result in the first non-empty result
  // container. Returns nullptr if no results exist.
  std::unique_ptr<ResultLocationDetails> GetFirstAvailableResultLocation()
      const;

  SearchResultBaseView* GetResultAtLocation(
      const ResultLocationDetails& location);

  // Returns the location of a result with the provided ID.
  // Returns nullptr if the result cannot be found.
  std::unique_ptr<ResultLocationDetails> FindResultWithId(
      const std::string& id);

  // Updates a |ResultLocationDetails| to a new container, updating most
  // attributes based on |result_selection_model_|.
  void ChangeContainer(ResultLocationDetails* location_details,
                       int new_container_index);

  // Container views to be traversed by this controller.
  // Owned by |SearchResultPageView|.
  raw_ptr<const ResultSelectionModel> result_selection_model_;

  // Returns true if the container at the given |index| within
  // |result_selection_model_| responds true to
  // |SearchResultContainerView|->|IsHorizontallyTraversable|.
  bool IsContainerAtIndexHorizontallyTraversable(int index) const;

  // The callback run when the selected result changes (including when the
  // selected result is cleared).
  base::RepeatingClosure selection_change_callback_;

  // The currently selected result view.
  raw_ptr<SearchResultBaseView, DanglingUntriaged> selected_result_ = nullptr;

  // The currently selected result ID.
  std::string selected_result_id_;

  // If set, any attempt to change current selection will be rejected.
  bool block_selection_changes_ = false;

  // The |ResultLocationDetails| for the currently selected result view
  std::unique_ptr<ResultLocationDetails> selected_location_details_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_RESULT_SELECTION_CONTROLLER_H_
