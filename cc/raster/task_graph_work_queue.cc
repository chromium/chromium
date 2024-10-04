// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/task_graph_work_queue.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <unordered_map>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"

namespace cc {
namespace {

bool CompareTaskPriority(const TaskGraphWorkQueue::PrioritizedTask& a,
                         const TaskGraphWorkQueue::PrioritizedTask& b) {
  // In this system, numerically lower priority is run first.
  return a.priority > b.priority;
}

class CompareTaskNamespacePriority {
 public:
  explicit CompareTaskNamespacePriority(uint16_t category)
      : category_(category) {}

  bool operator()(const TaskGraphWorkQueue::TaskNamespace* a,
                  const TaskGraphWorkQueue::TaskNamespace* b) {
    DCHECK(!a->ready_to_run_tasks.at(category_).empty());
    DCHECK(!b->ready_to_run_tasks.at(category_).empty());

    // Compare based on task priority of the ready_to_run_tasks heap .front()
    // will hold the max element of the heap, except after pop_heap, when max
    // element is moved to .back().
    return CompareTaskPriority(a->ready_to_run_tasks.at(category_).front(),
                               b->ready_to_run_tasks.at(category_).front());
  }

 private:
  uint16_t category_;
};

// Helper class for iterating over all dependents of a task.
class DependentIterator {
 public:
  DependentIterator(TaskGraph* graph, const Task* task)
      : graph_(graph),
        task_(task),
        current_index_(static_cast<size_t>(-1)),
        current_node_(nullptr) {
    ++(*this);
  }

  TaskGraph::Node& operator->() const {
    DCHECK_LT(current_index_, graph_->edges.size());
    DCHECK_EQ(graph_->edges[current_index_].task, task_);
    DCHECK(current_node_);
    return *current_node_;
  }

  TaskGraph::Node& operator*() const {
    DCHECK_LT(current_index_, graph_->edges.size());
    DCHECK_EQ(graph_->edges[current_index_].task, task_);
    DCHECK(current_node_);
    return *current_node_;
  }

  // Note: Performance can be improved by keeping edges sorted.
  DependentIterator& operator++() {
    // Find next dependency edge for |task_|.
    do {
      ++current_index_;
      if (current_index_ == graph_->edges.size())
        return *this;
    } while (graph_->edges[current_index_].task != task_);

    // Now find the node for the dependent of this edge.
    auto it = base::ranges::find(graph_->nodes,
                                 graph_->edges[current_index_].dependent.get(),
                                 &TaskGraph::Node::task);
    CHECK(it != graph_->nodes.end(), base::NotFatalUntil::M130);
    current_node_ = &(*it);

    return *this;
  }

  operator bool() const { return current_index_ < graph_->edges.size(); }

 private:
  // `graph_` and `task_` are not a raw_ptr<...> for performance reasons (based
  // on analysis of sampling profiler data and tab_search:top100:2020).
  RAW_PTR_EXCLUSION TaskGraph* graph_;
  RAW_PTR_EXCLUSION const Task* task_;

  size_t current_index_;

  // `current_node_` is not a raw_ptr<...> for performance reasons (based on
  // analysis of sampling profiler data and tab_search:top100:2020).
  RAW_PTR_EXCLUSION TaskGraph::Node* current_node_;
};

void RebuildNamespaceHeaps(TaskGraphWorkQueue::ReadyNamespaces& namespaces) {
  // Rearrange the task namespaces in |ready_to_run_namespaces| in such a
  // way that they form a heap.
  for (auto& it : namespaces) {
    uint16_t category = it.first;
    auto& task_namespace = it.second;
    std::make_heap(task_namespace.begin(), task_namespace.end(),
                   CompareTaskNamespacePriority(category));
  }
}

}  // namespace

TaskGraphWorkQueue::TaskNamespace::TaskNamespace() = default;

TaskGraphWorkQueue::TaskNamespace::TaskNamespace(TaskNamespace&& other) =
    default;

TaskGraphWorkQueue::TaskNamespace::~TaskNamespace() = default;

TaskGraphWorkQueue::TaskGraphWorkQueue() : next_namespace_id_(1) {}
TaskGraphWorkQueue::~TaskGraphWorkQueue() = default;

TaskGraphWorkQueue::PrioritizedTask::PrioritizedTask(
    scoped_refptr<Task> task,
    TaskNamespace* task_namespace,
    uint16_t category,
    uint16_t priority)
    : task(std::move(task)),
      task_namespace(task_namespace),
      category(category),
      priority(priority) {}

TaskGraphWorkQueue::PrioritizedTask::PrioritizedTask(PrioritizedTask&& other) =
    default;
TaskGraphWorkQueue::PrioritizedTask::~PrioritizedTask() = default;

NamespaceToken TaskGraphWorkQueue::GenerateNamespaceToken() {
  NamespaceToken token(next_namespace_id_++);
  DCHECK(!base::Contains(namespaces_, token));
  return token;
}

void TaskGraphWorkQueue::ScheduleTasks(NamespaceToken token, TaskGraph* graph) {
  TaskNamespace& task_namespace = namespaces_[token];

  // First adjust number of dependencies to reflect completed tasks.
  for (const scoped_refptr<Task>& task : task_namespace.completed_tasks) {
    for (DependentIterator node_it(graph, task.get()); node_it; ++node_it) {
      TaskGraph::Node& node = *node_it;
      DCHECK_LT(0u, node.dependencies);
      node.dependencies--;
    }
  }

  // Build new "ready to run" queue and remove nodes from old graph.
  for (auto& ready_to_run_tasks_it : task_namespace.ready_to_run_tasks) {
    ready_to_run_tasks_it.second.clear();
  }

  TRACE_EVENT(
      "toplevel", "cc::TaskGraphWorkQueue::ScheduleTasks",
      [&](perfetto::EventContext ctx) {
        for (const TaskGraph::Node& node : graph->nodes) {
          DCHECK(node.task->trace_task_id() != 0)
              << "Every raster task should be associated with a task id.\n";
          ctx.event()->add_flow_ids(node.task->trace_task_id());
        }
      });

  for (const TaskGraph::Node& node : graph->nodes) {
    // Remove any old nodes that are associated with this task. The result is
    // that the old graph is left with all nodes not present in this graph,
    // which we use below to determine what tasks need to be canceled.
    auto old_it = base::ranges::find(task_namespace.graph.nodes, node.task,
                                     &TaskGraph::Node::task);
    if (old_it != task_namespace.graph.nodes.end()) {
      std::swap(*old_it, task_namespace.graph.nodes.back());
      // If old task is scheduled to run again and not yet started running,
      // reset its state to initial state as it has to be inserted in new
      // |ready_to_run_tasks|, where it gets scheduled.
      if (node.task->state().IsScheduled())
        node.task->state().Reset();
      task_namespace.graph.nodes.pop_back();
    }

    // Task is not ready to run if dependencies are not yet satisfied.
    if (node.dependencies)
      continue;

    // Skip if already finished running task.
    if (node.task->state().IsFinished())
      continue;

    // Skip if already running.
    if (base::Contains(task_namespace.running_tasks, node.task.get(),
                       &CategorizedTask::second)) {
      continue;
    }

    node.task->state().DidSchedule();
    task_namespace.ready_to_run_tasks[node.category].emplace_back(
        node.task, &task_namespace, node.category, node.priority);
  }

  // Rearrange the elements in each vector within |ready_to_run_tasks| in such a
  // way that they form a heap.
  for (auto& it : task_namespace.ready_to_run_tasks) {
    auto& ready_to_run_tasks = it.second;
    std::make_heap(ready_to_run_tasks.begin(), ready_to_run_tasks.end(),
                   CompareTaskPriority);
  }

  // Swap task graph.
  task_namespace.graph.Swap(graph);

  // Determine what tasks in old graph need to be canceled.
  for (auto it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
    TaskGraph::Node& node = *it;

    // Skip if already finished running task.
    if (node.task->state().IsFinished())
      continue;

    // Skip if already running.
    if (base::Contains(task_namespace.running_tasks, node.task.get(),
                       &CategorizedTask::second)) {
      continue;
    }

    DCHECK(!base::Contains(task_namespace.completed_tasks, node.task.get()));
    node.task->state().DidCancel();
    task_namespace.completed_tasks.push_back(node.task);
  }

  // Build new "ready to run" task namespaces queue.
  for (auto& ready_to_run_namespaces_it : ready_to_run_namespaces_) {
    ready_to_run_namespaces_it.second.clear();
  }
  for (auto& namespace_it : namespaces_) {
    auto& task_namespace_to_check = namespace_it.second;
    for (auto& ready_to_run_tasks_it :
         task_namespace_to_check.ready_to_run_tasks) {
      auto& ready_to_run_tasks = ready_to_run_tasks_it.second;
      uint16_t category = ready_to_run_tasks_it.first;
      if (!ready_to_run_tasks.empty()) {
        ready_to_run_namespaces_[category].push_back(&task_namespace_to_check);
      }
    }
  }

  RebuildNamespaceHeaps(ready_to_run_namespaces_);
}

TaskGraphWorkQueue::PrioritizedTask TaskGraphWorkQueue::GetNextTaskToRun(
    uint16_t category) {
  TaskNamespace::Vector& ready_to_run_namespaces =
      ready_to_run_namespaces_[category];
  DCHECK(!ready_to_run_namespaces.empty());

  // Take top priority TaskNamespace from |ready_to_run_namespaces|.
  std::pop_heap(ready_to_run_namespaces.begin(), ready_to_run_namespaces.end(),
                CompareTaskNamespacePriority(category));
  TaskNamespace* task_namespace = ready_to_run_namespaces.back();
  ready_to_run_namespaces.pop_back();

  PrioritizedTask::Vector& ready_to_run_tasks =
      task_namespace->ready_to_run_tasks[category];
  DCHECK(!ready_to_run_tasks.empty());

  // Take top priority task from |ready_to_run_tasks|.
  std::pop_heap(ready_to_run_tasks.begin(), ready_to_run_tasks.end(),
                CompareTaskPriority);
  PrioritizedTask task = std::move(ready_to_run_tasks.back());
  ready_to_run_tasks.pop_back();

  // Add task namespace back to |ready_to_run_namespaces| if not empty after
  // taking top priority task.
  if (!ready_to_run_tasks.empty()) {
    ready_to_run_namespaces.push_back(task_namespace);
    std::push_heap(ready_to_run_namespaces.begin(),
                   ready_to_run_namespaces.end(),
                   CompareTaskNamespacePriority(category));
  }

  // Add task to |running_tasks|.
  task.task->state().DidStart();
  task_namespace->running_tasks.push_back(
      std::make_pair(task.category, task.task));

  return task;
}

bool TaskGraphWorkQueue::DecrementNodeDependencies(
    TaskGraph::Node& node,
    TaskNamespace* task_namespace) {
  DCHECK_LT(0u, node.dependencies);
  node.dependencies--;
  // Task is ready if it has no dependencies and is in the new state, Add it
  // to |ready_to_run_tasks_|.
  if (!node.dependencies && node.task->state().IsNew()) {
    PrioritizedTask::Vector& ready_to_run_tasks =
        task_namespace->ready_to_run_tasks[node.category];

    bool was_empty = ready_to_run_tasks.empty();
    node.task->state().DidSchedule();
    ready_to_run_tasks.emplace_back(node.task, task_namespace, node.category,
                                    node.priority);
    std::push_heap(ready_to_run_tasks.begin(), ready_to_run_tasks.end(),
                   CompareTaskPriority);

    // Task namespace is ready if it has at least one ready to run task. Add
    // it to |ready_to_run_namespaces_| if it just become ready.
    if (was_empty) {
      TaskNamespace::Vector& ready_to_run_namespaces =
          ready_to_run_namespaces_[node.category];

      DCHECK(!base::Contains(ready_to_run_namespaces, task_namespace));
      ready_to_run_namespaces.push_back(task_namespace);
    }
    return true;
  }
  return false;
}

void TaskGraphWorkQueue::CompleteTask(PrioritizedTask completed_task) {
  TaskNamespace* task_namespace = completed_task.task_namespace;
  scoped_refptr<Task> task(std::move(completed_task.task));

  // Remove task from |running_tasks|.
  auto it = base::ranges::find(task_namespace->running_tasks, task,
                               &CategorizedTask::second);
  CHECK(it != task_namespace->running_tasks.end(), base::NotFatalUntil::M130);
  std::swap(*it, task_namespace->running_tasks.back());
  task_namespace->running_tasks.pop_back();

  // Now iterate over all dependents to decrement dependencies and check if they
  // are ready to run.
  bool ready_to_run_namespaces_has_heap_properties = true;
  for (DependentIterator dependent_it(&task_namespace->graph, task.get());
       dependent_it; ++dependent_it) {
    ready_to_run_namespaces_has_heap_properties &=
        !DecrementNodeDependencies(*dependent_it, task_namespace);
  }

  // Rearrange the task namespaces in |ready_to_run_namespaces_| in such a way
  // that they yet again form a heap.
  if (!ready_to_run_namespaces_has_heap_properties) {
    RebuildNamespaceHeaps(ready_to_run_namespaces_);
  }

  // Finally add task to |completed_tasks|.
  task->state().DidFinish();
  task_namespace->completed_tasks.push_back(std::move(task));
}

void TaskGraphWorkQueue::CollectCompletedTasks(NamespaceToken token,
                                               Task::Vector* completed_tasks) {
  auto it = namespaces_.find(token);
  if (it == namespaces_.end())
    return;

  TaskNamespace& task_namespace = it->second;

  DCHECK_EQ(0u, completed_tasks->size());
  completed_tasks->swap(task_namespace.completed_tasks);
  if (!HasFinishedRunningTasksInNamespace(&task_namespace))
    return;

  // Remove namespace if finished running tasks.
  DCHECK_EQ(0u, task_namespace.completed_tasks.size());
  DCHECK(!HasReadyToRunTasksInNamespace(&task_namespace));
  DCHECK_EQ(0u, task_namespace.running_tasks.size());
  namespaces_.erase(it);
}

bool TaskGraphWorkQueue::DependencyMismatch(const TaskGraph* graph) {
  // Value storage will be 0-initialized.
  std::unordered_map<const Task*, size_t> dependents;
  for (const TaskGraph::Edge& edge : graph->edges)
    dependents[edge.dependent]++;

  for (const TaskGraph::Node& node : graph->nodes) {
    if (dependents[node.task.get()] != node.dependencies)
      return true;
  }

  return false;
}

}  // namespace cc
