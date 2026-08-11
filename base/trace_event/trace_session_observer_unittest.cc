// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_session_observer.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::trace_event {

class StartObserverOnly : public TraceSessionObserver {
 public:
  StartObserverOnly() = default;
  ~StartObserverOnly() override = default;

  void OnStart(const perfetto::DataSourceBase::StartArgs&) override {
    on_start_called_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  // Do NOT override WillClearIncrementalState

  bool on_start_called() const { return on_start_called_; }
  bool will_clear_called() const { return will_clear_called_; }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void Reset() {
    on_start_called_ = false;
    will_clear_called_ = false;
    quit_closure_.Reset();
  }

 private:
  bool on_start_called_ = false;
  bool will_clear_called_ = false;
  base::OnceClosure quit_closure_;
};

class IncrementalObserverOnly : public TraceSessionObserver {
 public:
  IncrementalObserverOnly() = default;
  ~IncrementalObserverOnly() override = default;

  // Do NOT override OnStart

  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override {
    will_clear_called_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool on_start_called() const { return on_start_called_; }
  bool will_clear_called() const { return will_clear_called_; }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void Reset() {
    on_start_called_ = false;
    will_clear_called_ = false;
    quit_closure_.Reset();
  }

 private:
  bool on_start_called_ = false;
  bool will_clear_called_ = false;
  base::OnceClosure quit_closure_;
};

class ComboObserver : public TraceSessionObserver {
 public:
  ComboObserver() = default;
  ~ComboObserver() override = default;

  void OnStart(const perfetto::DataSourceBase::StartArgs&) override {
    on_start_called_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override {
    will_clear_called_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool on_start_called() const { return on_start_called_; }
  bool will_clear_called() const { return will_clear_called_; }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void Reset() {
    on_start_called_ = false;
    will_clear_called_ = false;
    quit_closure_.Reset();
  }

 private:
  bool on_start_called_ = false;
  bool will_clear_called_ = false;
  base::OnceClosure quit_closure_;
};

class TraceSessionObserverListTest : public testing::Test {
 protected:
  TraceSessionObserverListTest() = default;
  ~TraceSessionObserverListTest() override = default;

  TraceSessionObserverList& GetList() {
    return TraceSessionObserverList::GetInstance();
  }

  MainThreadObserverHolder* GetMainThreadHolder() {
    return &GetList().main_thread_holder_;
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(TraceSessionObserverListTest, CompileTimeDetection) {
  // Ensure the singleton is initialized.
  TraceSessionObserverList::Initialize();
  TraceSessionObserverList& list = GetList();

  StartObserverOnly start_observer;
  IncrementalObserverOnly incremental_observer;
  ComboObserver combo_observer;

  // AddObserver is templated, so it will detect the overrides of each concrete
  // class.
  TraceSessionObserverList::AddObserver(&start_observer);
  TraceSessionObserverList::AddObserver(&incremental_observer);
  TraceSessionObserverList::AddObserver(&combo_observer);

  EXPECT_TRUE(
      GetMainThreadHolder()->start_observers_.HasObserver(&start_observer));
  EXPECT_TRUE(
      GetMainThreadHolder()->start_observers_.HasObserver(&combo_observer));
  EXPECT_FALSE(GetMainThreadHolder()->start_observers_.HasObserver(
      &incremental_observer));

  EXPECT_FALSE(GetMainThreadHolder()->incremental_state_observers_.HasObserver(
      &start_observer));
  EXPECT_TRUE(GetMainThreadHolder()->incremental_state_observers_.HasObserver(
      &combo_observer));
  EXPECT_TRUE(GetMainThreadHolder()->incremental_state_observers_.HasObserver(
      &incremental_observer));

  // 1. Test OnStart notification.
  {
    base::RunLoop run_loop;
    combo_observer.set_quit_closure(run_loop.QuitClosure());

    perfetto::DataSourceBase::StartArgs args;
    list.OnStart(args);
    run_loop.Run();

    EXPECT_TRUE(start_observer.on_start_called());
    EXPECT_FALSE(start_observer.will_clear_called());

    EXPECT_FALSE(incremental_observer.on_start_called());
    EXPECT_FALSE(incremental_observer.will_clear_called());

    EXPECT_TRUE(combo_observer.on_start_called());
    EXPECT_FALSE(combo_observer.will_clear_called());
  }

  start_observer.Reset();
  incremental_observer.Reset();
  combo_observer.Reset();

  // 2. Test WillClearIncrementalState notification.
  {
    base::RunLoop run_loop;
    combo_observer.set_quit_closure(run_loop.QuitClosure());

    perfetto::DataSourceBase::ClearIncrementalStateArgs args;
    list.WillClearIncrementalState(args);
    run_loop.Run();

    EXPECT_FALSE(start_observer.on_start_called());
    EXPECT_FALSE(start_observer.will_clear_called());

    EXPECT_FALSE(incremental_observer.on_start_called());
    EXPECT_TRUE(incremental_observer.will_clear_called());

    EXPECT_FALSE(combo_observer.on_start_called());
    EXPECT_TRUE(combo_observer.will_clear_called());
  }

  // RemoveObserver is also templated and must be called with the same types to
  // detect correctly.
  TraceSessionObserverList::RemoveObserver(&start_observer);
  TraceSessionObserverList::RemoveObserver(&incremental_observer);
  TraceSessionObserverList::RemoveObserver(&combo_observer);

  EXPECT_FALSE(
      GetMainThreadHolder()->start_observers_.HasObserver(&start_observer));
  EXPECT_FALSE(
      GetMainThreadHolder()->start_observers_.HasObserver(&combo_observer));
  EXPECT_FALSE(GetMainThreadHolder()->incremental_state_observers_.HasObserver(
      &incremental_observer));
  EXPECT_FALSE(GetMainThreadHolder()->incremental_state_observers_.HasObserver(
      &combo_observer));
}

}  // namespace base::trace_event
