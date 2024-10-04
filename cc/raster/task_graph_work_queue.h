// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_
#define CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_

#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "cc/cc_export.h"
#include "cc/raster/task_graph_runner.h"

namespace cc {

// Implements a queue of incoming TaskGraph work. Designed for use by
// implementations of TaskGraphRunner. Not thread safe, so the caller is
// responsible for all necessary locking.
//
// Tasks in the queue are divided into categories. Tasks from a single graph may
// be put into different categories, each of which is prioritized independently
// from the others. It is up to the implementation of TaskGraphRunner to
// define the meaning of the categories and handle them appropriately.
class CC_EXPORT TaskGraphWorkQueue {
 public:
  struct TaskNamespace;

  struct CC_EXPORT PrioritizedTask {
    typedef std::vector<PrioritizedTask> Vector;

    PrioritizedTask(scoped_refptr<Task> task,
                    TaskNamespace* task_namespace,
                    uint16_t category,
                    uint16_t priority);
    PrioritizedTask(const PrioritizedTask&) = delete;
    PrioritizedTask(PrioritizedTask&& other);
    ~PrioritizedTask();

    PrioritizedTask& operator=(const PrioritizedTask&) = delete;
    PrioritizedTask& operator=(PrioritizedTask&& other) = default;

    scoped_refptr<Task> task;
    raw_ptr<TaskNamespace> task_namespace;
    uint16_t category;
    uint16_t priority;
  };

  using CategorizedTask = std::pair<uint16_t, scoped_refptr<Task>>;

  // Helper classes and static methods used by dependent classes.
  struct TaskNamespace {
    using Vector = std::vector<raw_ptr<TaskNamespace, VectorExperimental>>;
    using ReadyTasks = std::map<uint16_t, PrioritizedTask::Vector>;

    TaskNamespace();
    TaskNamespace(const TaskNamespace&) = delete;
    TaskNamespace(TaskNamespace&& other);
    ~TaskNamespace();

    TaskNamespace& operator=(const TaskNamespace&) = delete;
    TaskNamespace& operator=(TaskNamespace&&) = default;

    // Current task graph.
    TaskGraph graph;

    // Map from category to a vector of tasks that are ready to run for that
    // category.
    ReadyTasks ready_to_run_tasks;

    // Completed tasks not yet collected by origin thread.
    Task::Vector completed_tasks;

    // This set contains all currently running tasks.
    std::vector<CategorizedTask> running_tasks;
  };

  using ReadyNamespaces = std::map<uint16_t, TaskNamespace::Vector>;

  TaskGraphWorkQueue();
  TaskGraphWorkQueue(const TaskGraphWorkQueue&) = delete;
  virtual ~TaskGraphWorkQueue();

  TaskGraphWorkQueue& operator=(const TaskGraphWorkQueue&) = delete;

  // Generates a NamespaceToken which is guaranteed to be unique within this
  // TaskGraphWorkQueue.
  NamespaceToken GenerateNamespaceToken();

  // Updates a TaskNamespace with a new TaskGraph to run. This cancels any
  // previous tasks in the graph being replaced.
  void ScheduleTasks(NamespaceToken token, TaskGraph* graph);

  // Returns the next task to run for the given category.
  PrioritizedTask GetNextTaskToRun(uint16_t category);

  // Marks a task as completed, adding it to its namespace's list of completed
  // tasks and updating the list of |ready_to_run_namespaces|.
  void CompleteTask(PrioritizedTask completed_task);

  // Helper which populates a vector of completed tasks from the provided
  // namespace.
  void CollectCompletedTasks(NamespaceToken token,
                             Task::Vector* completed_tasks);

  // Helper which returns the raw TaskNamespace* for the given token. Used to
  // allow callers to re-use a TaskNamespace*, reducing the number of lookups
  // needed.
  TaskNamespace* GetNamespaceForToken(NamespaceToken token) {
    auto it = namespaces_.find(token);
    if (it == namespaces_.end())
      return nullptr;
    return &it->second;
  }

  static bool HasReadyToRunTasksInNamespace(
      const TaskNamespace* task_namespace) {
    return !base::ranges::all_of(
        task_namespace->ready_to_run_tasks, &PrioritizedTask::Vector::empty,
        &TaskNamespace::ReadyTasks::value_type::second);
  }

  static bool HasFinishedRunningTasksInNamespace(
      const TaskNamespace* task_namespace) {
    return task_namespace->running_tasks.empty() &&
           !HasReadyToRunTasksInNamespace(task_namespace);
  }

  bool HasReadyToRunTasks() const {
    return !base::ranges::all_of(ready_to_run_namespaces_,
                                 &TaskNamespace::Vector::empty,
                                 &ReadyNamespaces::value_type::second);
  }

  bool HasReadyToRunTasksForCategory(uint16_t category) const {
    auto found = ready_to_run_namespaces_.find(category);
    return found != ready_to_run_namespaces_.end() && !found->second.empty();
  }

  bool HasAnyNamespaces() const { return !namespaces_.empty(); }

  bool HasFinishedRunningTasksInAllNamespaces() {
    return base::ranges::all_of(
        namespaces_, [](const TaskNamespaceMap::value_type& entry) {
          return HasFinishedRunningTasksInNamespace(&entry.second);
        });
  }

  const ReadyNamespaces& ready_to_run_namespaces() const {
    return ready_to_run_namespaces_;
  }

  size_t NumRunningTasksForCategory(uint16_t category) const {
    size_t count = 0;
    for (const auto& task_namespace_entry : namespaces_) {
      for (const auto& categorized_task :
           task_namespace_entry.second.running_tasks) {
        if (categorized_task.first == category) {
          ++count;
        }
      }
    }
    return count;
  }

  size_t NumReadyTasksForCategory(uint16_t category) const {
    auto found = ready_to_run_namespaces_.find(category);
    if (found == ready_to_run_namespaces_.end())
      return 0;
    size_t count = 0;
    for (TaskGraphWorkQueue::TaskNamespace* task_namespace_entry :
         found->second) {
      DCHECK(
          base::Contains(task_namespace_entry->ready_to_run_tasks, category));
      count += task_namespace_entry->ready_to_run_tasks.at(category).size();
    }
    return count;
  }

  // Helper function which ensures that graph dependencies were correctly
  // configured.
  static bool DependencyMismatch(const TaskGraph* graph);

 private:
  bool DecrementNodeDependencies(TaskGraph::Node& node,
                                 TaskNamespace* task_namespace);

  // Helper class used to provide NamespaceToken comparison to TaskNamespaceMap.
  class CompareToken {
   public:
    bool operator()(const NamespaceToken& lhs,
                    const NamespaceToken& rhs) const {
      return lhs.id_ < rhs.id_;
    }
  };

  using TaskNamespaceMap =
      std::map<NamespaceToken, TaskNamespace, CompareToken>;

  TaskNamespaceMap namespaces_;

  // Map from category to a vector of ready to run namespaces for that category.
  ReadyNamespaces ready_to_run_namespaces_;

  // Provides a unique id to each NamespaceToken.
  int next_namespace_id_;
};

}  // namespace cc

#endif  // CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_
