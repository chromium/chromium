// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_
#define CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_

#include <stdint.h>

#include <algorithm>
#include <map>
#include <vector>

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
    TaskNamespace* task_namespace;
    uint16_t category;
    uint16_t priority;
  };

  using CategorizedTask = std::pair<uint16_t, scoped_refptr<Task>>;

  // Helper classes and static methods used by dependent classes.
  struct TaskNamespace {
    typedef std::vector<TaskNamespace*> Vector;

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
    std::map<uint16_t, PrioritizedTask::Vector> ready_to_run_tasks;

    // Completed tasks not yet collected by origin thread.
    Task::Vector completed_tasks;

    // This set contains all currently running tasks.
    std::vector<CategorizedTask> running_tasks;
  };

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
    return std::find_if(
               task_namespace->ready_to_run_tasks.begin(),
               task_namespace->ready_to_run_tasks.end(),
               [](const std::pair<const uint16_t, PrioritizedTask::Vector>&
                      ready_to_run_tasks) {
                 return !ready_to_run_tasks.second.empty();
               }) != task_namespace->ready_to_run_tasks.end();
  }

  static bool HasFinishedRunningTasksInNamespace(
      const TaskNamespace* task_namespace) {
    return task_namespace->running_tasks.empty() &&
           !HasReadyToRunTasksInNamespace(task_namespace);
  }

  bool HasReadyToRunTasks() const {
    return std::find_if(
               ready_to_run_namespaces_.begin(), ready_to_run_namespaces_.end(),
               [](const std::pair<const uint16_t, TaskNamespace::Vector>&
                      ready_to_run_namespaces) {
                 return !ready_to_run_namespaces.second.empty();
               }) != ready_to_run_namespaces_.end();
  }

  bool HasReadyToRunTasksForCategory(uint16_t category) const {
    auto found = ready_to_run_namespaces_.find(category);
    return found != ready_to_run_namespaces_.end() && !found->second.empty();
  }

  bool HasAnyNamespaces() const { return !namespaces_.empty(); }

  bool HasFinishedRunningTasksInAllNamespaces() {
    return std::find_if(
               namespaces_.begin(), namespaces_.end(),
               [](const TaskNamespaceMap::value_type& entry) {
                 return !HasFinishedRunningTasksInNamespace(&entry.second);
               }) == namespaces_.end();
  }

  const std::map<uint16_t, TaskNamespace::Vector>& ready_to_run_namespaces()
      const {
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

  // Helper function which ensures that graph dependencies were correctly
  // configured.
  static bool DependencyMismatch(const TaskGraph* graph);

 private:
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
  std::map<uint16_t, TaskNamespace::Vector> ready_to_run_namespaces_;

  // Provides a unique id to each NamespaceToken.
  int next_namespace_id_;
};

}  // namespace cc

#endif  // CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_
