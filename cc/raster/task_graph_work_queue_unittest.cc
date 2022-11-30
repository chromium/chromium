// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/task_graph_work_queue.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakeTaskImpl : public Task {
 public:
  FakeTaskImpl() = default;
  FakeTaskImpl(const FakeTaskImpl&) = delete;

  FakeTaskImpl& operator=(const FakeTaskImpl&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {}

 private:
  ~FakeTaskImpl() override = default;
};

TEST(TaskGraphWorkQueueTest, TestChangingDependency) {
  TaskGraphWorkQueue work_queue;
  NamespaceToken token = work_queue.GenerateNamespaceToken();

  // Create a graph with one task
  TaskGraph graph1;
  scoped_refptr<FakeTaskImpl> task(new FakeTaskImpl());
  graph1.nodes.push_back(TaskGraph::Node(task.get(), 0u, 0u, 0u));

  // Schedule the graph
  work_queue.ScheduleTasks(token, &graph1);

  // Run the task.
  TaskGraphWorkQueue::PrioritizedTask prioritized_task =
      work_queue.GetNextTaskToRun(0u);
  work_queue.CompleteTask(std::move(prioritized_task));

  // Create a graph where task1 has a dependency
  TaskGraph graph2;
  scoped_refptr<FakeTaskImpl> dependency_task(new FakeTaskImpl());
  graph2.nodes.push_back(TaskGraph::Node(task.get(), 0u, 0u, 1u));
  graph2.nodes.push_back(TaskGraph::Node(dependency_task.get(), 0u, 0u, 0u));
  graph2.edges.push_back(TaskGraph::Edge(dependency_task.get(), task.get()));

  // Schedule the second graph.
  work_queue.ScheduleTasks(token, &graph2);

  // Run the |dependency_task|
  TaskGraphWorkQueue::PrioritizedTask prioritized_dependency_task =
      work_queue.GetNextTaskToRun(0u);
  EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task.get());
  work_queue.CompleteTask(std::move(prioritized_dependency_task));

  // We should have no tasks to run, as the dependent task already completed.
  EXPECT_FALSE(work_queue.HasReadyToRunTasks());
}

// Tasks with same priority but in different category.
TEST(TaskGraphWorkQueueTest, TestTaskWithDifferentCategory) {
  TaskGraphWorkQueue work_queue;
  NamespaceToken token = work_queue.GenerateNamespaceToken();

  // Create a graph where | task| has dependencies.
  TaskGraph graph;
  scoped_refptr<FakeTaskImpl> task(new FakeTaskImpl());
  scoped_refptr<FakeTaskImpl> dependency_task1(new FakeTaskImpl());
  scoped_refptr<FakeTaskImpl> dependency_task2(new FakeTaskImpl());
  scoped_refptr<FakeTaskImpl> dependency_task3(new FakeTaskImpl());

  graph.nodes.push_back(TaskGraph::Node(task.get(), 0u, 0u, 3u));
  graph.nodes.push_back(TaskGraph::Node(dependency_task1.get(), 0u, 0u, 0u));
  graph.nodes.push_back(TaskGraph::Node(dependency_task2.get(), 1u, 0u, 0u));
  graph.nodes.push_back(TaskGraph::Node(dependency_task3.get(), 2u, 0u, 0u));

  graph.edges.push_back(TaskGraph::Edge(dependency_task1.get(), task.get()));
  graph.edges.push_back(TaskGraph::Edge(dependency_task2.get(), task.get()));
  graph.edges.push_back(TaskGraph::Edge(dependency_task3.get(), task.get()));

  // Schedule the graph.
  work_queue.ScheduleTasks(token, &graph);

  // Run the |dependency_task1|from category 0.
  TaskGraphWorkQueue::PrioritizedTask prioritized_dependency_task =
      work_queue.GetNextTaskToRun(0u);
  EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task1.get());
  work_queue.CompleteTask(std::move(prioritized_dependency_task));
  EXPECT_TRUE(work_queue.HasReadyToRunTasks());
  EXPECT_FALSE(work_queue.HasReadyToRunTasksForCategory(0u));
  EXPECT_TRUE(work_queue.HasReadyToRunTasksForCategory(1u));
  EXPECT_TRUE(work_queue.HasReadyToRunTasksForCategory(2u));

  // Run the |dependency_task2|from category 1.
  prioritized_dependency_task = work_queue.GetNextTaskToRun(1u);
  EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task2.get());
  work_queue.CompleteTask(std::move(prioritized_dependency_task));
  EXPECT_TRUE(work_queue.HasReadyToRunTasks());
  EXPECT_FALSE(work_queue.HasReadyToRunTasksForCategory(0u));
  EXPECT_FALSE(work_queue.HasReadyToRunTasksForCategory(1u));
  EXPECT_TRUE(work_queue.HasReadyToRunTasksForCategory(2u));

  // Run the |dependency_task3|from category 2.
  prioritized_dependency_task = work_queue.GetNextTaskToRun(2u);
  EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task3.get());
  work_queue.CompleteTask(std::move(prioritized_dependency_task));
  EXPECT_TRUE(work_queue.HasReadyToRunTasks());
  // Once all dependencies from different category completed, | task| turns
  // ready to run.
  EXPECT_TRUE(work_queue.HasReadyToRunTasksForCategory(0u));
  EXPECT_FALSE(work_queue.HasReadyToRunTasksForCategory(1u));
  EXPECT_FALSE(work_queue.HasReadyToRunTasksForCategory(2u));

  prioritized_dependency_task = work_queue.GetNextTaskToRun(0u);
  EXPECT_EQ(prioritized_dependency_task.task.get(), task.get());
  work_queue.CompleteTask(std::move(prioritized_dependency_task));
  EXPECT_FALSE(work_queue.HasReadyToRunTasks());
}

// Tasks with different priority run in a priority order. But need to guarantee
// its dependences are completed.
TEST(TaskGraphWorkQueueTest, TestTaskWithDifferentPriority) {
  TaskGraphWorkQueue work_queue;
  NamespaceToken token = work_queue.GenerateNamespaceToken();
  {
    // Create a graph where task has a dependency
    TaskGraph graph;
    scoped_refptr<FakeTaskImpl> task(new FakeTaskImpl());
    scoped_refptr<FakeTaskImpl> dependency_task1(new FakeTaskImpl());
    scoped_refptr<FakeTaskImpl> dependency_task2(new FakeTaskImpl());
    scoped_refptr<FakeTaskImpl> dependency_task3(new FakeTaskImpl());

    // | task| has the lowest priority and 3 dependences, will run last.
    graph.nodes.push_back(TaskGraph::Node(task.get(), 0u, 2u, 3u));
    graph.nodes.push_back(TaskGraph::Node(dependency_task1.get(), 0u, 3u, 0u));
    graph.nodes.push_back(TaskGraph::Node(dependency_task2.get(), 0u, 2u, 0u));
    graph.nodes.push_back(TaskGraph::Node(dependency_task3.get(), 0u, 1u, 0u));

    graph.edges.push_back(TaskGraph::Edge(dependency_task1.get(), task.get()));
    graph.edges.push_back(TaskGraph::Edge(dependency_task2.get(), task.get()));
    graph.edges.push_back(TaskGraph::Edge(dependency_task3.get(), task.get()));

    // Schedule the graph.
    work_queue.ScheduleTasks(token, &graph);

    // Run the |dependency_task|
    TaskGraphWorkQueue::PrioritizedTask prioritized_dependency_task =
        work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task3.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_TRUE(work_queue.HasReadyToRunTasks());

    prioritized_dependency_task = work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task2.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_TRUE(work_queue.HasReadyToRunTasks());

    prioritized_dependency_task = work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task1.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_TRUE(work_queue.HasReadyToRunTasks());

    // | task| runs last.
    prioritized_dependency_task = work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), task.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_FALSE(work_queue.HasReadyToRunTasks());
  }

  {
    // Create a graph where task has dependencies
    TaskGraph graph;
    scoped_refptr<FakeTaskImpl> task(new FakeTaskImpl());
    scoped_refptr<FakeTaskImpl> dependency_task1(new FakeTaskImpl());
    scoped_refptr<FakeTaskImpl> dependency_task2(new FakeTaskImpl());
    scoped_refptr<FakeTaskImpl> dependency_task3(new FakeTaskImpl());

    // | task| has the highest priority and 3 dependences, also will run last.
    graph.nodes.push_back(TaskGraph::Node(task.get(), 0u, 0u, 3u));
    graph.nodes.push_back(TaskGraph::Node(dependency_task1.get(), 0u, 3u, 0u));
    graph.nodes.push_back(TaskGraph::Node(dependency_task2.get(), 0u, 2u, 0u));
    graph.nodes.push_back(TaskGraph::Node(dependency_task3.get(), 0u, 1u, 0u));

    graph.edges.push_back(TaskGraph::Edge(dependency_task1.get(), task.get()));
    graph.edges.push_back(TaskGraph::Edge(dependency_task2.get(), task.get()));
    graph.edges.push_back(TaskGraph::Edge(dependency_task3.get(), task.get()));

    // Schedule the graph.
    work_queue.ScheduleTasks(token, &graph);

    // Run the |dependency_task|
    TaskGraphWorkQueue::PrioritizedTask prioritized_dependency_task =
        work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task3.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_TRUE(work_queue.HasReadyToRunTasks());

    prioritized_dependency_task = work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task2.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_TRUE(work_queue.HasReadyToRunTasks());

    prioritized_dependency_task = work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), dependency_task1.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_TRUE(work_queue.HasReadyToRunTasks());

    // | task| runs last.
    prioritized_dependency_task = work_queue.GetNextTaskToRun(0u);
    EXPECT_EQ(prioritized_dependency_task.task.get(), task.get());
    work_queue.CompleteTask(std::move(prioritized_dependency_task));
    EXPECT_FALSE(work_queue.HasReadyToRunTasks());
  }
}

}  // namespace
}  // namespace cc
