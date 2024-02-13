// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/graphs_container_view.h"

#include <numeric>

#include "ash/hud_display/cpu_graph_page_view.h"
#include "ash/hud_display/fps_graph_page_view.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/memory_graph_page_view.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace hud_display {
namespace {

// UI refresh interval.
constexpr base::TimeDelta kGraphsDataRefreshInterval = base::Milliseconds(500);

void GetDataSnapshotOnThreadPool(DataSource* data_source,
                                 DataSource::Snapshot* out_snapshot) {
  // This is run on the ThreadPool.
  *out_snapshot = data_source->GetSnapshotAndReset();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GraphsContainerView, public:

BEGIN_METADATA(GraphsContainerView)
END_METADATA

GraphsContainerView::GraphsContainerView()
    : start_time_(base::TimeTicks::Now()),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      data_source_(new DataSource,
                   base::OnTaskRunnerDeleter(file_task_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // Make all graph pages take the whole view and make sure that only one
  // is shown at a time.
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Adds another graphs page.
  AddChildView(
      std::make_unique<MemoryGraphPageView>(kGraphsDataRefreshInterval))
      ->SetID(static_cast<int>(HUDDisplayMode::MEMORY));
  AddChildView(std::make_unique<CpuGraphPageView>(kGraphsDataRefreshInterval))
      ->SetID(static_cast<int>(HUDDisplayMode::CPU));
  AddChildView(std::make_unique<FPSGraphPageView>(kGraphsDataRefreshInterval))
      ->SetID(static_cast<int>(HUDDisplayMode::FPS));

  RequestDataUpdate();
}

GraphsContainerView::~GraphsContainerView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void GraphsContainerView::RequestDataUpdate() {
  std::unique_ptr<DataSource::Snapshot> snapshot_container =
      std::make_unique<DataSource::Snapshot>();
  DataSource::Snapshot* snapshot = snapshot_container.get();
  file_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetDataSnapshotOnThreadPool,
                     base::Unretained(data_source_.get()), snapshot),
      base::BindOnce(&GraphsContainerView::UpdateData,
                     weak_factory_.GetWeakPtr(),
                     std::move(snapshot_container)));
}

void GraphsContainerView::UpdateData(
    std::unique_ptr<DataSource::Snapshot> snapshot) {
  // Adjust for any missing data.
  const off_t expected_updates =
      (base::TimeTicks::Now() - start_time_) / kGraphsDataRefreshInterval;
  const unsigned intervals =
      expected_updates > static_cast<off_t>(data_update_count_)
          ? expected_updates - data_update_count_
          : 1;
  data_update_count_ += intervals;

  for (views::View* child : children()) {
    // Insert missing points.
    for (unsigned j = 0; j < intervals; ++j)
      static_cast<GraphPageViewBase*>(child)->UpdateData(*snapshot);
  }

  SchedulePaint();

  const base::TimeTicks next_start_time =
      start_time_ + kGraphsDataRefreshInterval * data_update_count_;
  const base::TimeTicks now = base::TimeTicks::Now();
  if (next_start_time <= now) {
    RequestDataUpdate();
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GraphsContainerView::RequestDataUpdate,
                       weak_factory_.GetWeakPtr()),
        next_start_time - now);
  }
}

void GraphsContainerView::SetMode(HUDDisplayMode mode) {
  auto* selected = GetViewByID(static_cast<int>(mode));
  if (!selected) {
    DCHECK(selected);
    return;
  }
  for (views::View* child : children()) {
    child->SetVisible(false);
  }

  selected->SetVisible(true);
}

}  // namespace hud_display
}  // namespace ash
