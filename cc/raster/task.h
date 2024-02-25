// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_H_
#define CC_RASTER_TASK_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"

namespace cc {
class Task;

// This class provides states to manage life cycle of a task and given below is
// how it is used by TaskGraphWorkQueue to process life cycle of a task.
// Task is in kNew state when it is created. When task is added to
// |ready_to_run_tasks| then its state is changed to kScheduled. Task can be
// canceled from kNew state (not yet scheduled to run) or from kScheduled state,
// when new ScheduleTasks() is triggered and its state is changed to kCanceled.
// When task is about to run it is added |running_tasks| and its state is
// changed to kRunning. Once task finishes running, its state is changed to
// kFinished. Both kCanceled and kFinished tasks are added to |completed_tasks|.
//                ╔═════╗
//         +------║ kNew║------+
//         |      ╚═════╝      |
//         v                   v
//   ┌───────────┐        ╔══════════╗
//   │ kScheduled│------> ║ kCanceled║
//   └───────────┘        ╚══════════╝
//         |
//         v
//    ┌─────────┐         ╔══════════╗
//    │ kRunning│-------> ║ kFinished║
//    └─────────┘         ╚══════════╝
class CC_EXPORT TaskState {
 public:
  bool IsNew() const;
  bool IsScheduled() const;
  bool IsRunning() const;
  bool IsFinished() const;
  bool IsCanceled() const;

  // Functions to change the state of task. These functions should be called
  // only from TaskGraphWorkQueue where the life cycle of a task is decided or
  // from tests. These functions are not thread-safe. Caller is responsible for
  // thread safety.
  void Reset();  // Sets state to kNew.
  void DidSchedule();
  void DidStart();
  void DidFinish();
  void DidCancel();

  std::string ToString() const;

 private:
  friend class Task;

  // Let only Task class create the TaskState.
  TaskState();
  ~TaskState();

  enum class Value : uint16_t {
    kNew,
    kScheduled,
    kRunning,
    kFinished,
    kCanceled
  };

  Value value_;
};

// A task which can be run by a TaskGraphRunner. To run a Task, it should be
// inserted into a TaskGraph, which can then be scheduled on the
// TaskGraphRunner.
class CC_EXPORT Task : public base::RefCountedThreadSafe<Task> {
 public:
  typedef std::vector<scoped_refptr<Task>> Vector;

  TaskState& state() { return state_; }
  void set_frame_number(int64_t frame_number) { frame_number_ = frame_number; }
  int64_t frame_number() { return frame_number_; }

  // Unique trace flow id for the given task, used to connect the places where
  // the task was posted from and the task itself.
  uint64_t trace_task_id() { return trace_task_id_; }
  void set_trace_task_id(uint64_t id) { trace_task_id_ = id; }

  // Subclasses should implement this method. RunOnWorkerThread may be called
  // on any thread, and subclasses are responsible for locking and thread
  // safety.
  virtual void RunOnWorkerThread() = 0;

 protected:
  friend class base::RefCountedThreadSafe<Task>;

  Task();
  virtual ~Task();

 private:
  TaskState state_;
  int64_t frame_number_ = -1;
  int64_t trace_task_id_ = 0;
};

// A task dependency graph describes the order in which to execute a set
// of tasks. Dependencies are represented as edges. Each node is assigned
// a category, a priority and a run count that matches the number of
// dependencies. Priority range from 0 (most favorable scheduling) to UINT16_MAX
// (least favorable). Categories range from 0 to UINT16_MAX. It is up to the
// implementation and its consumer to determine the meaning (if any) of a
// category. A TaskGraphRunner implementation may chose to prioritize certain
// categories over others, regardless of the individual priorities of tasks.
struct CC_EXPORT TaskGraph {
  struct CC_EXPORT Node {
    typedef std::vector<Node> Vector;

    Node(scoped_refptr<Task> new_task,
         uint16_t category,
         uint16_t priority,
         uint32_t dependencies);
    Node(const Node&) = delete;
    Node(Node&& other);
    ~Node();

    Node& operator=(const Node&) = delete;
    Node& operator=(Node&& other) = default;

    scoped_refptr<Task> task;
    uint16_t category;
    uint16_t priority;
    uint32_t dependencies;
  };

  struct Edge {
    typedef std::vector<Edge> Vector;

    Edge(const Task* task, Task* dependent)
        : task(task), dependent(dependent) {}

    raw_ptr<const Task, AcrossTasksDanglingUntriaged> task;
    raw_ptr<Task, AcrossTasksDanglingUntriaged> dependent;
  };

  TaskGraph();
  TaskGraph(const TaskGraph&) = delete;
  TaskGraph(TaskGraph&& other);
  ~TaskGraph();

  TaskGraph& operator=(const TaskGraph&) = delete;
  TaskGraph& operator=(TaskGraph&&) = default;

  void Swap(TaskGraph* other);
  void Reset();

  Node::Vector nodes;
  Edge::Vector edges;
};

}  // namespace cc

#endif  // CC_RASTER_TASK_H_
