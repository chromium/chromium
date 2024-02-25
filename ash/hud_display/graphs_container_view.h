// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_
#define ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_

#include "ash/hud_display/data_source.h"
#include "ash/hud_display/graph_page_view_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

enum class HUDDisplayMode;

// GraphsContainerView class draws a bunch of graphs.
class GraphsContainerView : public views::View {
  METADATA_HEADER(GraphsContainerView, views::View)

 public:
  GraphsContainerView();
  GraphsContainerView(const GraphsContainerView&) = delete;
  GraphsContainerView& operator=(const GraphsContainerView&) = delete;
  ~GraphsContainerView() override;


  // Updates graphs display to match given mode.
  void SetMode(HUDDisplayMode mode);

  // Schedules new data update on the thread pool.
  void RequestDataUpdate();

  // Update graphs data from the given snapshot.
  void UpdateData(std::unique_ptr<DataSource::Snapshot> snapshot);

 private:
  // This helps detect missing data intervals.
  const base::TimeTicks start_time_;
  size_t data_update_count_{0};

  // Source of graphs data.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::unique_ptr<DataSource, base::OnTaskRunnerDeleter> data_source_;

  SEQUENCE_CHECKER(ui_sequence_checker_);

  base::WeakPtrFactory<GraphsContainerView> weak_factory_{this};
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_
