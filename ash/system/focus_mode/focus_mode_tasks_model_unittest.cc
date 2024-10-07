// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tasks_model.h"

#include <optional>

#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::Optional;
using testing::Pointee;
using testing::SizeIs;

MATCHER_P(SameId, id, "") {
  *result_listener << "where the id is " << (arg.task_id.id);
  return id == arg.task_id.id;
}

constexpr char kTaskListId[] = "task_list_id";

base::Time::Exploded Date(int year, int month, int day) {
  base::Time::Exploded exploded;
  exploded.year = year;
  exploded.month = month;
  exploded.day_of_month = day;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 1;
  exploded.millisecond = 1;
  return exploded;
}

class RecordingObserver : public FocusModeTasksModel::Observer {
 public:
  RecordingObserver() = default;

  void OnSelectedTaskChanged(
      const std::optional<FocusModeTask>& selected_task) override {
    last_selected_task_ = selected_task;
  }
  void OnTasksUpdated(const std::vector<FocusModeTask>& tasks) override {
    last_task_list_ = tasks;
  }
  void OnTaskCompleted(const FocusModeTask& completed_task) override {
    last_completed_task_ = completed_task;
  }

  void Reset() {
    last_selected_task_.reset();
    last_task_list_.reset();
    last_completed_task_.reset();
  }

  const std::optional<FocusModeTask>& last_selected_task() const {
    return last_selected_task_;
  }
  const std::optional<std::vector<FocusModeTask>>& last_task_list() const {
    return last_task_list_;
  }
  const std::optional<FocusModeTask>& last_completed_task() const {
    return last_completed_task_;
  }

 private:
  std::optional<FocusModeTask> last_selected_task_;
  std::optional<std::vector<FocusModeTask>> last_task_list_;
  std::optional<FocusModeTask> last_completed_task_;
};

class FakeDelegate final : public FocusModeTasksModel::Delegate {
 public:
  FakeDelegate() = default;

  MOCK_METHOD(void,
              AddTask,
              (const FocusModeTasksModel::TaskUpdate& update,
               FocusModeTasksModel::Delegate::FetchTaskCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateTask,
              (const FocusModeTasksModel::TaskUpdate& update),
              (override));
  MOCK_METHOD(void, FetchTasks, (), (override));
  MOCK_METHOD(void,
              FetchTask,
              (const TaskId& task_id,
               FocusModeTasksModel::Delegate::FetchTaskCallback callback),
              (override));

  base::WeakPtr<FakeDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeDelegate> weak_ptr_factory_{this};
};

class FocusModeTasksModelObserverTest : public testing::Test {
 public:
  FocusModeTasksModelObserverTest() = default;

  void SetUp() override { tasks_model_.AddObserver(&observer_); }

  void TearDown() override { tasks_model_.RemoveObserver(&observer_); }

  FocusModeTasksModel& model() { return tasks_model_; }

  RecordingObserver& observer() { return observer_; }

 private:
  RecordingObserver observer_;
  FocusModeTasksModel tasks_model_;
};

FocusModeTask TestTask(std::string_view task_id) {
  FocusModeTask task;
  task.title = "Task Title";
  task.task_id = {.list_id = kTaskListId, .id = std::string{task_id}};
  return task;
}

TEST_F(FocusModeTasksModelObserverTest, SelectedTaskChanged_EmptyList) {
  FocusModeTask task = TestTask("task0");
  model().SetSelectedTask(task);
  EXPECT_THAT(observer().last_selected_task(), Optional(SameId("task0")));
  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(1)));
}

TEST_F(FocusModeTasksModelObserverTest, SelectedTaskChanged_SelectById) {
  std::vector<FocusModeTask> test_tasks = {TestTask("task0"), TestTask("task1"),
                                           TestTask("task2")};
  model().SetTaskList(std::move(test_tasks));

  EXPECT_THAT(observer().last_selected_task(), Eq(std::nullopt));
  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(3)));

  ASSERT_TRUE(model().SetSelectedTask({.list_id = kTaskListId, .id = "task1"}));
  EXPECT_THAT(observer().last_selected_task(), Optional(SameId("task1")));
}

TEST_F(FocusModeTasksModelObserverTest,
       SelectedTaskChanged_SelectById_NotInList) {
  std::vector<FocusModeTask> test_tasks = {TestTask("task0"), TestTask("task1"),
                                           TestTask("task2")};
  model().SetTaskList(std::move(test_tasks));

  EXPECT_FALSE(
      model().SetSelectedTask({.list_id = kTaskListId, .id = "NotInList"}));
  EXPECT_THAT(observer().last_selected_task(), Eq(std::nullopt));
}

TEST_F(FocusModeTasksModelObserverTest, SelectedTaskFromPrefs_PrefFirst) {
  model().SetSelectedTaskFromPrefs({
      .list_id = kTaskListId,
      .id = "from_prefs",
  });
  EXPECT_THAT(observer().last_selected_task(), Eq(std::nullopt));
  EXPECT_THAT(observer().last_task_list(), Eq(std::nullopt));

  // Update task list with an updated title.
  FocusModeTask updated_pref_task = TestTask("from_prefs");
  updated_pref_task.title = "Updated title";
  std::vector<FocusModeTask> test_tasks = {TestTask("task0"), TestTask("task1"),
                                           std::move(updated_pref_task),
                                           TestTask("task2")};
  model().SetTaskList(std::move(test_tasks));

  EXPECT_THAT(
      observer().last_selected_task(),
      Optional(testing::AllOf(SameId("from_prefs"),
                              Field(&FocusModeTask::title, "Updated title"))));
  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(4)));
}

TEST_F(FocusModeTasksModelObserverTest, SelectedTaskFromPrefs_ListFirst) {
  FocusModeTask pref_task = TestTask("from_prefs");
  pref_task.title = "Correct title";
  std::vector<FocusModeTask> test_tasks = {TestTask("task0"), TestTask("task1"),
                                           std::move(pref_task),
                                           TestTask("task2")};
  model().SetTaskList(std::move(test_tasks));

  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(4)));

  model().SetSelectedTaskFromPrefs(
      {.list_id = kTaskListId, .id = "from_prefs"});

  // The selected task should match what was in the list and not what's in
  // prefs.
  EXPECT_THAT(
      observer().last_selected_task(),
      Optional(testing::AllOf(SameId("from_prefs"),
                              Field(&FocusModeTask::title, "Correct title"))));
}

TEST_F(FocusModeTasksModelObserverTest, SelectedTaskFromPrefs_TaskIsCompleted) {
  // Desired task from prefs.
  TaskId task_id = {.list_id = kTaskListId, .id = "from_prefs"};

  // Setup.
  FakeDelegate delegate;
  model().SetDelegate(delegate.AsWeakPtr());

  FocusModeTasksModel::Delegate::FetchTaskCallback callback;
  EXPECT_CALL(delegate, FetchTask(Eq(task_id), _))
      .WillOnce(
          [&callback](
              const TaskId& task_id,
              FocusModeTasksModel::Delegate::FetchTaskCallback task_callback) {
            callback = std::move(task_callback);
          });

  // Set the selected task id from prefs.
  model().SetSelectedTaskFromPrefs(task_id);

  // Simulate response with completed task.
  FocusModeTask pref_task = TestTask("from_prefs");
  pref_task.title = "Completed Task";
  pref_task.completed = true;
  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run({pref_task});

  // The selected task should match what was in the list and not what's in
  // prefs.
  EXPECT_THAT(observer().last_selected_task(), Eq(std::nullopt));
  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(0)));
}

TEST_F(FocusModeTasksModelObserverTest,
       SelectedTaskFromPrefs_UpdateFromDelegate_TaskIsNew) {
  FakeDelegate delegate;
  model().SetDelegate(delegate.AsWeakPtr());

  FocusModeTasksModel::Delegate::FetchTaskCallback callback;
  EXPECT_CALL(delegate, FetchTask(_, _))
      .WillOnce(
          [&callback](
              const TaskId& task_id,
              FocusModeTasksModel::Delegate::FetchTaskCallback task_callback) {
            callback = std::move(task_callback);
          });

  TaskId task_id = {.list_id = kTaskListId, .id = "from_prefs"};
  model().SetSelectedTaskFromPrefs(task_id);

  FocusModeTask retrieved_task;
  retrieved_task.task_id = task_id;
  retrieved_task.title = "Retrieved task";
  base::Time::Exploded exploded = Date(2024, 06, 06);
  ASSERT_TRUE(base::Time::FromLocalExploded(exploded, &retrieved_task.updated));
  retrieved_task.completed = false;

  std::move(callback).Run({retrieved_task});

  EXPECT_THAT(observer().last_selected_task(),
              Optional(Field(&FocusModeTask::title, "Retrieved task")));
}

TEST_F(FocusModeTasksModelObserverTest,
       SelectedTaskFromPrefs_UpdateFromDelegate_TaskInList) {
  FakeDelegate delegate;
  model().SetDelegate(delegate.AsWeakPtr());

  FocusModeTasksModel::Delegate::FetchTaskCallback callback;
  EXPECT_CALL(delegate, FetchTask(_, _))
      .WillOnce(
          [&callback](
              const TaskId& task_id,
              FocusModeTasksModel::Delegate::FetchTaskCallback task_callback) {
            callback = std::move(task_callback);
          });

  // Select from_prefs task.
  TaskId task_id = {.list_id = kTaskListId, .id = "from_prefs"};
  model().SetSelectedTaskFromPrefs(task_id);

  // Set tasks with pref task.
  model().SetTaskList({TestTask("task0"), TestTask("task1"), TestTask("task2"),
                       TestTask("from_prefs")});

  // Create a task to be retrieved.
  FocusModeTask retrieved_task;
  retrieved_task.task_id = task_id;
  retrieved_task.title = "Retrieved task";
  base::Time::Exploded exploded = Date(2024, 06, 14);
  ASSERT_TRUE(base::Time::FromLocalExploded(exploded, &retrieved_task.updated));
  retrieved_task.completed = false;

  // Reset the observer since `SetTaskList()` would have triggered observers.
  observer().Reset();

  // Run callback with the retrieved task.
  std::move(callback).Run({retrieved_task});

  // The callback should not have run (since the task from list is preferred).
  // The title of the task should match that from `TestTask()`.
  EXPECT_THAT(observer().last_selected_task(), Eq(std::nullopt));
  EXPECT_THAT(model().selected_task(),
              testing::Pointee(testing::Not(
                  Field(&FocusModeTask::title, "Retrieved task"))));
}

TEST_F(FocusModeTasksModelObserverTest, ClearTask) {
  model().SetTaskList(
      {TestTask("task0"), TestTask("task1"), TestTask("task2")});
  ASSERT_TRUE(model().SetSelectedTask({.list_id = kTaskListId, .id = "task1"}));
  // Assert that there was a notification that a task was selected.
  ASSERT_TRUE(observer().last_selected_task().has_value());

  model().ClearSelectedTask();

  // We should get a nullopt when the selected task is cleared.
  EXPECT_THAT(observer().last_selected_task(), Eq(std::nullopt));
}

TEST_F(FocusModeTasksModelObserverTest, RequestUpdate_ImmediateNotification) {
  model().SetTaskList(
      {TestTask("task0"), TestTask("task1"), TestTask("task2")});
  ASSERT_TRUE(model().SetSelectedTask({.list_id = kTaskListId, .id = "task1"}));

  observer().Reset();

  // Requesting an update immediately triggers observers to be called.
  model().RequestUpdate();

  EXPECT_THAT(observer().last_selected_task(), Optional(SameId("task1")));
  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(3)));
}

TEST_F(FocusModeTasksModelObserverTest, CompleteTask) {
  FakeDelegate delegate;
  model().SetDelegate(delegate.AsWeakPtr());

  model().SetTaskList(
      {TestTask("task0"), TestTask("task1"), TestTask("task2")});

  model().SetSelectedTask({.list_id = kTaskListId, .id = "task1"});

  EXPECT_CALL(delegate, UpdateTask(_));
  model().UpdateTask(FocusModeTasksModel::TaskUpdate::CompletedUpdate(
      {.list_id = kTaskListId, .id = "task1"}));

  EXPECT_THAT(observer().last_completed_task(), Optional(SameId("task1")));
}

// Tests that we fetch the selected task data when `RequestUpdate()` is called
// and update the selected task title if it is updated.
TEST_F(FocusModeTasksModelObserverTest, FetchSelectedTask_TaskTitleUpdated) {
  // Setup.
  FakeDelegate delegate;
  model().SetDelegate(delegate.AsWeakPtr());
  TaskId task_id = {.list_id = kTaskListId, .id = "selected_task"};
  model().SetTaskList(
      {TestTask("task0"), TestTask("task1"), TestTask(task_id.id)});
  model().SetSelectedTask(task_id);

  FocusModeTasksModel::Delegate::FetchTaskCallback callback;
  EXPECT_CALL(delegate, FetchTask(Eq(task_id), _))
      .WillOnce(
          [&callback](
              const TaskId& task_id,
              FocusModeTasksModel::Delegate::FetchTaskCallback task_callback) {
            callback = std::move(task_callback);
          });
  EXPECT_CALL(delegate, FetchTasks());

  // Requesting an update immediately triggers observers to be called.
  model().RequestUpdate();

  // Simulate response with an updated task title.
  FocusModeTask updated_task = TestTask(task_id.id);
  updated_task.title = "Updated task title";
  updated_task.completed = false;
  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run({updated_task});

  EXPECT_THAT(observer().last_selected_task(), Optional(SameId(task_id.id)));
  EXPECT_THAT(model().selected_task(),
              Pointee(Field(&FocusModeTask::title, "Updated task title")));
}

// Tests that if a selected task was completed remotely, it is marked as
// completed and removed from the list for `RequestUpdate()`.
TEST_F(FocusModeTasksModelObserverTest, FetchSelectedTask_TaskIsCompleted) {
  // Setup.
  FakeDelegate delegate;
  model().SetDelegate(delegate.AsWeakPtr());
  TaskId task_id = {.list_id = kTaskListId, .id = "selected_task"};
  model().SetTaskList(
      {TestTask("task0"), TestTask("task1"), TestTask(task_id.id)});
  model().SetSelectedTask(task_id);

  FocusModeTasksModel::Delegate::FetchTaskCallback callback;
  EXPECT_CALL(delegate, FetchTask(Eq(task_id), _))
      .WillOnce(
          [&callback](
              const TaskId& task_id,
              FocusModeTasksModel::Delegate::FetchTaskCallback task_callback) {
            callback = std::move(task_callback);
          });
  EXPECT_CALL(delegate, UpdateTask(_));
  EXPECT_CALL(delegate, FetchTasks());

  // Requesting an update immediately triggers observers to be called.
  model().RequestUpdate();

  // Simulate response with completed task.
  FocusModeTask completed_task = TestTask(task_id.id);
  completed_task.title = "Completed Task";
  completed_task.completed = true;
  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run({completed_task});

  // The previously selected task should now be completed.
  EXPECT_THAT(observer().last_selected_task(), Eq(std::nullopt));
  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(2)));
  EXPECT_THAT(observer().last_completed_task(), Optional(SameId(task_id.id)));
}

// Tests that `FetchTasks()` is still called even if the selected task request
// fails.
TEST_F(FocusModeTasksModelObserverTest, FetchSelectedTask_RequestFails) {
  // Setup.
  FakeDelegate delegate;
  model().SetDelegate(delegate.AsWeakPtr());
  TaskId task_id = {.list_id = kTaskListId, .id = "selected_task"};
  model().SetTaskList(
      {TestTask("task0"), TestTask("task1"), TestTask(task_id.id)});
  model().SetSelectedTask(task_id);

  FocusModeTasksModel::Delegate::FetchTaskCallback callback;
  EXPECT_CALL(delegate, FetchTask(Eq(task_id), _))
      .WillOnce(
          [&callback](
              const TaskId& task_id,
              FocusModeTasksModel::Delegate::FetchTaskCallback task_callback) {
            callback = std::move(task_callback);
          });
  // We want to make sure that `FetchTasks()` is called every time, even when we
  // encounter a failure to keep the cache up to date.
  EXPECT_CALL(delegate, FetchTasks());

  // Requesting an update immediately triggers observers to be called.
  model().RequestUpdate();

  // Simulate a failed response.
  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run(FocusModeTask{});

  // Verify that nothing has changed.
  EXPECT_THAT(observer().last_selected_task(), Optional(SameId(task_id.id)));
  EXPECT_THAT(observer().last_task_list(), Optional(SizeIs(3)));
}

TEST(FocusModeTasksModelTest, RequestUpdate_CallsDelegate) {
  FakeDelegate delegate;

  FocusModeTasksModel tasks_model;
  tasks_model.SetDelegate(delegate.AsWeakPtr());

  EXPECT_CALL(delegate, FetchTasks());
  tasks_model.RequestUpdate();
}

TEST(FocusModeTasksModelTest, SetSelectedTask_NoTasks) {
  FocusModeTasksModel model;

  EXPECT_FALSE(
      model.SetSelectedTask({.list_id = kTaskListId, .id = "NotInList"}));
  EXPECT_THAT(model.selected_task(), Eq(nullptr));
}

TEST(FocusModeTasksModelTest, SetSelectedTask_OnlyItem) {
  FocusModeTasksModel model;
  model.SetTaskList({TestTask("task3")});

  model.SetSelectedTask({.list_id = kTaskListId, .id = "task3"});

  EXPECT_THAT(model.tasks(), testing::ElementsAre(SameId("task3")));
}

TEST(FocusModeTasksModelTest, SetSelectedTask_ReorderList) {
  FocusModeTasksModel model;

  model.SetTaskList({TestTask("task0"), TestTask("task1"), TestTask("task2"),
                     TestTask("task3"), TestTask("task4")});
  model.SetSelectedTask({.list_id = kTaskListId, .id = "task3"});

  EXPECT_THAT(
      model.tasks(),
      testing::ElementsAre(SameId("task3"), SameId("task0"), SameId("task1"),
                           SameId("task2"), SameId("task4")));
}

TEST(FocusModeTasksModelTest, SetSelectedTask_EmptyId) {
  FocusModeTasksModel model;

  FakeDelegate delegate;
  model.SetDelegate(delegate.AsWeakPtr());

  // If selected task has an empty `TaskId` the delegate should not be called.
  EXPECT_CALL(delegate, FetchTask).Times(0);

  TaskId empty_id;
  model.SetSelectedTaskFromPrefs(empty_id);
}

TEST(FocusModeTasksModelTest, UpdateTask_NewTask) {
  FakeDelegate delegate;
  FocusModeTasksModel model;
  model.SetDelegate(delegate.AsWeakPtr());

  model.SetTaskList({TestTask("task0"), TestTask("task1"), TestTask("task2")});

  model.SetSelectedTask({.list_id = kTaskListId, .id = "task1"});

  EXPECT_CALL(delegate, AddTask(_, _));
  model.UpdateTask(
      FocusModeTasksModel::TaskUpdate::NewTask("This is a new task"));

  // Verify that the id is new for now.
  EXPECT_THAT(model.selected_task()->task_id.pending, Eq(true));
  EXPECT_THAT(model.selected_task()->task_id.id, testing::Not(Eq("task1")));

  // The correct title is used in the new task.
  EXPECT_THAT(model.selected_task(),
              Pointee(Field(&FocusModeTask::title, "This is a new task")));

  // The new task is added to the list immediately.
  EXPECT_THAT(model.tasks(), SizeIs(4));

  // Verify that the new task is inserted at the front of the list.
  EXPECT_THAT(model.tasks()[0].task_id.pending, Eq(true));
  EXPECT_THAT(model.tasks()[0].title, Eq("This is a new task"));
}

TEST(FocusModeTasksModelTest, UpdateTask_NewTask_IdUpdated) {
  FakeDelegate delegate;
  FocusModeTasksModel model;
  model.SetDelegate(delegate.AsWeakPtr());

  FocusModeTasksModel::Delegate::FetchTaskCallback callback;
  EXPECT_CALL(delegate, AddTask(_, _))
      .WillOnce(
          [&callback](
              const FocusModeTasksModel::TaskUpdate&,
              FocusModeTasksModel::Delegate::FetchTaskCallback task_callback) {
            callback = std::move(task_callback);
          });

  model.UpdateTask(
      FocusModeTasksModel::TaskUpdate::NewTask("This is a new task"));

  FocusModeTask updated_task;
  updated_task.task_id = {.list_id = kTaskListId, .id = "new_task_id"};
  updated_task.title = "Should probably match";

  std::move(callback).Run(updated_task);

  EXPECT_THAT(model.selected_task(), Pointee(SameId("new_task_id")));
  EXPECT_THAT(model.tasks(), SizeIs(1));
}

TEST(FocusModeTasksModelTest, UpdateTask_CompleteTask) {
  FakeDelegate delegate;
  FocusModeTasksModel model;
  model.SetDelegate(delegate.AsWeakPtr());

  model.SetTaskList({TestTask("task0"), TestTask("task1"), TestTask("task2")});

  model.SetSelectedTask({.list_id = kTaskListId, .id = "task1"});

  EXPECT_CALL(delegate, UpdateTask(_));
  model.UpdateTask(FocusModeTasksModel::TaskUpdate::CompletedUpdate(
      {.list_id = kTaskListId, .id = "task1"}));

  EXPECT_THAT(model.selected_task(), Eq(nullptr));
  // Completed tasks are removed from the task list.
  EXPECT_THAT(model.tasks(), SizeIs(2));
}

TEST(FocusModeTasksModelTest, UpdateTask_EditTitle) {
  FakeDelegate delegate;
  FocusModeTasksModel model;
  model.SetDelegate(delegate.AsWeakPtr());

  model.SetTaskList({TestTask("task0"), TestTask("task1"), TestTask("task2")});

  TaskId task1_id = {.list_id = kTaskListId, .id = "task1"};
  model.SetSelectedTask(task1_id);

  EXPECT_CALL(delegate, UpdateTask(_));
  model.UpdateTask(FocusModeTasksModel::TaskUpdate::TitleUpdate(
      task1_id, "Updated task title"));

  EXPECT_THAT(model.selected_task(),
              Pointee(Field(&FocusModeTask::title, "Updated task title")));
  // Task list size should remain the same for an edit of an existing task.
  EXPECT_THAT(model.tasks(), SizeIs(3));
}

// Verify that we don't try to fetch non-existent tasks if an update is
// requested while the new task is still pending (b/371634351).
TEST(FocusModeTasksModelTest, NewTaskThenRequest) {
  FocusModeTasksModel model;

  FakeDelegate delegate;
  model.SetDelegate(delegate.AsWeakPtr());

  // The pending task will still be pending when `RequestUpdate()` so we
  // shouldn't attempt to fetch it (it doesn't have a valid id).
  EXPECT_CALL(delegate, FetchTask).Times(0);

  EXPECT_CALL(delegate, AddTask);
  model.UpdateTask(
      FocusModeTasksModel::TaskUpdate::NewTask("This is a new task"));

  // All tasks are requested.
  EXPECT_CALL(delegate, FetchTasks);
  model.RequestUpdate();
}

}  // namespace

void PrintTo(const FocusModeTask& task, std::ostream* os) {
  *os << "FocusModeTask(id: " << task.task_id.id << ", title: " << task.title
      << ")";
}

}  // namespace ash
