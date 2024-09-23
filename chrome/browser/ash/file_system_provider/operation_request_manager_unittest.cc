// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operation_request_manager.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

const RequestType kTestRequestType = RequestType::kGetMetadata;

std::optional<std::string> GetTestingParamFromResult(
    const RequestValue& result) {
  if (const std::string* testing_param = result.testing_params()) {
    return *testing_param;
  }
  return std::nullopt;
}

// Fake implementation for the notification manager. Simulates user action on
// a notification.
class FakeNotificationManager : public NotificationManagerInterface {
 public:
  FakeNotificationManager() = default;

  FakeNotificationManager(const FakeNotificationManager&) = delete;
  FakeNotificationManager& operator=(const FakeNotificationManager&) = delete;

  ~FakeNotificationManager() override = default;

  // NotificationManagerInterface overrides:
  void ShowUnresponsiveNotification(int id,
                                    NotificationCallback callback) override {
    callbacks_[id] = std::move(callback);
  }

  void HideUnresponsiveNotification(int id) override { callbacks_.erase(id); }

  // Aborts all of the virtually shown notifications.
  void Abort() { OnNotificationResult(ABORT); }

  // Discards all of the virtually shown notifications.
  void Continue() { OnNotificationResult(CONTINUE); }

  // Returns number of currently shown notifications.
  size_t size() { return callbacks_.size(); }

 private:
  typedef std::map<int, NotificationCallback> CallbackMap;

  // Handles a notification result by calling all registered callbacks and
  // clearing the list.
  void OnNotificationResult(NotificationResult result) {
    CallbackMap::iterator it = callbacks_.begin();
    while (it != callbacks_.end()) {
      CallbackMap::iterator current_it = it++;
      NotificationCallback callback = std::move(current_it->second);
      callbacks_.erase(current_it);
      std::move(callback).Run(result);
    }
  }

  CallbackMap callbacks_;
};

class TimeoutWaiter : public RequestManager::Observer {
 public:
  explicit TimeoutWaiter(RequestManager* request_manager) {
    scoped_observation_.Observe(request_manager);
  }
  void OnRequestCreated(int request_id, RequestType type) override {}
  void OnRequestDestroyed(int request_id,
                          OperationCompletion completion) override {}
  void OnRequestExecuted(int request_id) override {}
  void OnRequestFulfilled(int request_id,
                          const RequestValue& result,
                          bool has_more) override {}
  void OnRequestRejected(int request_id,
                         const RequestValue& result,
                         base::File::Error error) override {}
  void OnRequestTimedOut(int request_id) override { run_loop_.Quit(); }
  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<RequestManager, RequestManager::Observer>
      scoped_observation_{this};
};

// Logs calls of the success and error callbacks on requests.
class EventLogger {
 public:
  class ExecuteEvent {
   public:
    explicit ExecuteEvent(int request_id) : request_id_(request_id) {}
    virtual ~ExecuteEvent() = default;

    int request_id() { return request_id_; }

   private:
    int request_id_;
  };

  class SuccessEvent {
   public:
    SuccessEvent(int request_id, const RequestValue& result, bool has_more)
        : request_id_(request_id),
          testing_param_(GetTestingParamFromResult(result)),
          result_is_valid_(result.is_valid()),
          has_more_(has_more) {}
    virtual ~SuccessEvent() = default;

    int request_id() const { return request_id_; }
    const std::optional<std::string>& testing_param() const {
      return testing_param_;
    }
    bool has_more() const { return has_more_; }
    bool result_is_valid() const { return result_is_valid_; }

   private:
    int request_id_;
    std::optional<std::string> testing_param_;
    bool result_is_valid_;
    bool has_more_;
  };

  class ErrorEvent {
   public:
    ErrorEvent(int request_id, base::File::Error error)
        : request_id_(request_id), error_(error) {}
    virtual ~ErrorEvent() = default;

    int request_id() { return request_id_; }
    base::File::Error error() { return error_; }

   private:
    int request_id_;
    base::File::Error error_;
  };

  class AbortEvent {
   public:
    explicit AbortEvent(int request_id) : request_id_(request_id) {}
    virtual ~AbortEvent() = default;

    int request_id() { return request_id_; }

   private:
    int request_id_;
  };

  EventLogger() = default;

  EventLogger(const EventLogger&) = delete;
  EventLogger& operator=(const EventLogger&) = delete;

  virtual ~EventLogger() = default;

  void OnExecute(int request_id) {
    execute_events_.push_back(std::make_unique<ExecuteEvent>(request_id));
  }

  void OnSuccess(int request_id, const RequestValue& result, bool has_more) {
    success_events_.push_back(
        std::make_unique<SuccessEvent>(request_id, result, has_more));
  }

  void OnError(int request_id, base::File::Error error) {
    error_events_.push_back(std::make_unique<ErrorEvent>(request_id, error));
  }

  void OnAbort(int request_id) {
    abort_events_.push_back(std::make_unique<AbortEvent>(request_id));
  }

  std::vector<std::unique_ptr<ExecuteEvent>>& execute_events() {
    return execute_events_;
  }
  std::vector<std::unique_ptr<SuccessEvent>>& success_events() {
    return success_events_;
  }
  std::vector<std::unique_ptr<ErrorEvent>>& error_events() {
    return error_events_;
  }
  std::vector<std::unique_ptr<AbortEvent>>& abort_events() {
    return abort_events_;
  }

  base::WeakPtr<EventLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<std::unique_ptr<ExecuteEvent>> execute_events_;
  std::vector<std::unique_ptr<SuccessEvent>> success_events_;
  std::vector<std::unique_ptr<ErrorEvent>> error_events_;
  std::vector<std::unique_ptr<AbortEvent>> abort_events_;
  base::WeakPtrFactory<EventLogger> weak_ptr_factory_{this};
};

// Fake handler, which forwards callbacks to the logger. The handler is owned
// by a request manager, however the logger is owned by tests.
class FakeHandler : public RequestManager::HandlerInterface {
 public:
  // The handler can outlive the passed logger, so using a weak pointer. The
  // |execute_reply| value will be returned for the Execute() call.
  FakeHandler(base::WeakPtr<EventLogger> logger, bool execute_reply)
      : logger_(logger), execute_reply_(execute_reply) {}

  // RequestManager::Handler overrides.
  bool Execute(int request_id) override {
    if (logger_.get())
      logger_->OnExecute(request_id);

    return execute_reply_;
  }

  // RequestManager::Handler overrides.
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override {
    if (logger_.get())
      logger_->OnSuccess(request_id, std::move(result), has_more);
  }

  // RequestManager::Handler overrides.
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override {
    if (logger_.get())
      logger_->OnError(request_id, error);
  }

  void OnAbort(int request_id) override {
    if (logger_.get()) {
      logger_->OnAbort(request_id);
    }
  }

  FakeHandler(const FakeHandler&) = delete;
  FakeHandler& operator=(const FakeHandler&) = delete;

  ~FakeHandler() override = default;

 private:
  base::WeakPtr<EventLogger> logger_;
  bool execute_reply_;
};

// Observer the request manager for request events.
class RequestObserver : public RequestManager::Observer {
 public:
  class Event {
   public:
    explicit Event(int request_id) : request_id_(request_id) {}
    virtual ~Event() = default;
    int request_id() const { return request_id_; }

   private:
    int request_id_;
  };

  class CreatedEvent : public Event {
   public:
    CreatedEvent(int request_id, RequestType type)
        : Event(request_id), type_(type) {}
    ~CreatedEvent() override = default;

    RequestType type() const { return type_; }

   private:
    RequestType type_;
  };

  class FulfilledEvent : public Event {
   public:
    FulfilledEvent(int request_id, bool has_more)
        : Event(request_id), has_more_(has_more) {}
    ~FulfilledEvent() override = default;

    bool has_more() const { return has_more_; }

   private:
    bool has_more_;
  };

  class RejectedEvent : public Event {
   public:
    RejectedEvent(int request_id, base::File::Error error)
        : Event(request_id), error_(error) {}
    ~RejectedEvent() override = default;

    base::File::Error error() const { return error_; }

   private:
    base::File::Error error_;
  };

  class DestroyedEvent : public Event {
   public:
    DestroyedEvent(int request_id, OperationCompletion completion)
        : Event(request_id), completion_(completion) {}

    OperationCompletion completion() const { return completion_; }

   private:
    OperationCompletion completion_;
  };

  RequestObserver() = default;

  RequestObserver(const RequestObserver&) = delete;
  RequestObserver& operator=(const RequestObserver&) = delete;

  ~RequestObserver() override = default;

  // RequestManager::Observer overrides.
  void OnRequestCreated(int request_id, RequestType type) override {
    created_.emplace_back(request_id, type);
  }

  // RequestManager::Observer overrides.
  void OnRequestDestroyed(int request_id,
                          OperationCompletion completion) override {
    destroyed_.emplace_back(request_id, completion);
  }

  // RequestManager::Observer overrides.
  void OnRequestExecuted(int request_id) override {
    executed_.emplace_back(request_id);
  }

  // RequestManager::Observer overrides.
  void OnRequestFulfilled(int request_id,
                          const RequestValue& result,
                          bool has_more) override {
    fulfilled_.emplace_back(request_id, has_more);
  }

  // RequestManager::Observer overrides.
  void OnRequestRejected(int request_id,
                         const RequestValue& result,
                         base::File::Error error) override {
    rejected_.emplace_back(request_id, error);
  }

  // RequestManager::Observer overrides.
  void OnRequestTimedOut(int request_id) override {
    timed_out_.emplace_back(request_id);
  }

  const std::vector<CreatedEvent>& created() const { return created_; }
  const std::vector<DestroyedEvent>& destroyed() const { return destroyed_; }
  const std::vector<Event>& executed() const { return executed_; }
  const std::vector<FulfilledEvent>& fulfilled() const { return fulfilled_; }
  const std::vector<RejectedEvent>& rejected() const { return rejected_; }
  const std::vector<Event>& timed_out() const { return timed_out_; }

 private:
  std::vector<CreatedEvent> created_;
  std::vector<DestroyedEvent> destroyed_;
  std::vector<Event> executed_;
  std::vector<FulfilledEvent> fulfilled_;
  std::vector<RejectedEvent> rejected_;
  std::vector<Event> timed_out_;
};

}  // namespace

class FileSystemProviderRequestManagerTest : public testing::Test {
 protected:
  FileSystemProviderRequestManagerTest() = default;
  ~FileSystemProviderRequestManagerTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    notification_manager_ = std::make_unique<FakeNotificationManager>();
    request_manager_ = std::make_unique<OperationRequestManager>(
        profile_.get(), /*provider_id=*/std::string(),
        notification_manager_.get(), /*timeout=*/base::Seconds(10));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FakeNotificationManager> notification_manager_;
  std::unique_ptr<OperationRequestManager> request_manager_;
};

TEST_F(FileSystemProviderRequestManagerTest, CreateFailure) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/false)));

  EXPECT_EQ(0, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  EXPECT_EQ(1u, observer.created().size());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());
  EXPECT_EQ(1u, observer.destroyed().size());
  EXPECT_EQ(0u, observer.executed().size());

  const std::vector<int> active_request_ids =
      request_manager_->GetActiveRequestIds();
  EXPECT_EQ(0u, active_request_ids.size());

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest, CreateAndFulFill) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  const std::vector<int> active_request_ids =
      request_manager_->GetActiveRequestIds();
  ASSERT_EQ(1u, active_request_ids.size());
  EXPECT_EQ(request_id, active_request_ids[0]);

  RequestValue response(RequestValue::CreateForTesting("i-like-vanilla"));
  const bool has_more = false;

  const base::File::Error result = request_manager_->FulfillRequest(
      request_id, std::move(response), has_more);
  EXPECT_EQ(base::File::FILE_OK, result);

  ASSERT_EQ(1u, observer.fulfilled().size());
  EXPECT_EQ(request_id, observer.fulfilled()[0].request_id());
  EXPECT_FALSE(observer.fulfilled()[0].has_more());

  // Validate if the callback has correct arguments.
  ASSERT_EQ(1u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EventLogger::SuccessEvent* event = logger.success_events()[0].get();
  ASSERT_TRUE(event->testing_param());
  EXPECT_EQ("i-like-vanilla", *event->testing_param());
  EXPECT_FALSE(event->has_more());

  // Confirm, that the request is removed. Basically, fulfilling again for the
  // same request, should fail.
  {
    EXPECT_EQ(0u, request_manager_->GetActiveRequestIds().size());

    const base::File::Error retry =
        request_manager_->FulfillRequest(request_id, RequestValue(), has_more);
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, retry);
    EXPECT_EQ(1u, observer.fulfilled().size());
  }

  // Rejecting should also fail.
  {
    const base::File::Error retry = request_manager_->RejectRequest(
        request_id, RequestValue(), base::File::FILE_ERROR_FAILED);
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, retry);
    EXPECT_EQ(0u, observer.rejected().size());
  }

  ASSERT_EQ(1u, observer.destroyed().size());
  EXPECT_EQ(request_id, observer.destroyed()[0].request_id());
  EXPECT_EQ(OperationCompletion::kCompletedNormally,
            observer.destroyed()[0].completion());
  EXPECT_EQ(0u, observer.timed_out().size());

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest, CreateAndFulFill_WithHasNext) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  const bool has_more = true;

  const base::File::Error result =
      request_manager_->FulfillRequest(request_id, RequestValue(), has_more);
  EXPECT_EQ(base::File::FILE_OK, result);

  // Validate if the callback has correct arguments.
  ASSERT_EQ(1u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EventLogger::SuccessEvent* event = logger.success_events()[0].get();
  EXPECT_FALSE(event->result_is_valid());
  EXPECT_TRUE(event->has_more());

  ASSERT_EQ(1u, observer.fulfilled().size());
  EXPECT_EQ(request_id, observer.fulfilled()[0].request_id());
  EXPECT_TRUE(observer.fulfilled()[0].has_more());

  // Confirm, that the request is not removed (since it has has_more == true).
  // Basically, fulfilling again for the same request, should not fail.
  {
    const std::vector<int> active_request_ids =
        request_manager_->GetActiveRequestIds();
    ASSERT_EQ(1u, active_request_ids.size());
    EXPECT_EQ(request_id, active_request_ids[0]);

    const bool new_has_more = false;
    const base::File::Error retry = request_manager_->FulfillRequest(
        request_id, RequestValue(), new_has_more);
    EXPECT_EQ(base::File::FILE_OK, retry);

    ASSERT_EQ(2u, observer.fulfilled().size());
    EXPECT_EQ(request_id, observer.fulfilled()[1].request_id());
    EXPECT_FALSE(observer.fulfilled()[1].has_more());
  }

  // Since |new_has_more| is false, then the request should be removed. To check
  // it, try to fulfill again, what should fail.
  {
    const std::vector<int> active_request_ids =
        request_manager_->GetActiveRequestIds();
    EXPECT_EQ(0u, active_request_ids.size());

    const bool new_has_more = false;
    const base::File::Error retry = request_manager_->FulfillRequest(
        request_id, RequestValue(), new_has_more);
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, retry);
    EXPECT_EQ(0u, observer.rejected().size());
  }

  ASSERT_EQ(1u, observer.destroyed().size());
  EXPECT_EQ(request_id, observer.destroyed()[0].request_id());
  EXPECT_EQ(0u, observer.timed_out().size());

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest, CreateAndReject) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  const base::File::Error error = base::File::FILE_ERROR_NO_MEMORY;
  const base::File::Error result =
      request_manager_->RejectRequest(request_id, RequestValue(), error);
  EXPECT_EQ(base::File::FILE_OK, result);

  // Validate if the callback has correct arguments.
  ASSERT_EQ(1u, logger.error_events().size());
  EXPECT_EQ(0u, logger.success_events().size());
  EventLogger::ErrorEvent* event = logger.error_events()[0].get();
  EXPECT_EQ(error, event->error());

  ASSERT_EQ(1u, observer.rejected().size());
  EXPECT_EQ(request_id, observer.rejected()[0].request_id());
  EXPECT_EQ(error, observer.rejected()[0].error());

  // Confirm, that the request is removed. Basically, fulfilling again for the
  // same request, should fail.
  {
    const bool has_more = false;
    const base::File::Error retry =
        request_manager_->FulfillRequest(request_id, RequestValue(), has_more);
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, retry);
    EXPECT_EQ(0u, observer.fulfilled().size());
  }

  // Rejecting should also fail.
  {
    const base::File::Error retry =
        request_manager_->RejectRequest(request_id, RequestValue(), error);
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, retry);
    EXPECT_EQ(1u, observer.rejected().size());
  }

  ASSERT_EQ(1u, observer.destroyed().size());
  EXPECT_EQ(request_id, observer.destroyed()[0].request_id());
  EXPECT_EQ(OperationCompletion::kCompletedNormally,
            observer.destroyed()[0].completion());
  EXPECT_EQ(0u, observer.timed_out().size());

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest,
       CreateAndFulfillWithWrongRequestId) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  const bool has_more = true;

  const base::File::Error result = request_manager_->FulfillRequest(
      request_id + 1, RequestValue(), has_more);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, result);

  // Callbacks should not be called.
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.success_events().size());

  EXPECT_EQ(0u, observer.fulfilled().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  // Confirm, that the request hasn't been removed, by fulfilling it correctly.
  {
    const base::File::Error retry =
        request_manager_->FulfillRequest(request_id, RequestValue(), has_more);
    EXPECT_EQ(base::File::FILE_OK, retry);
    EXPECT_EQ(1u, observer.fulfilled().size());
  }

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest,
       CreateAndRejectWithWrongRequestId) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  const base::File::Error error = base::File::FILE_ERROR_NO_MEMORY;
  const base::File::Error result =
      request_manager_->RejectRequest(request_id + 1, RequestValue(), error);
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, result);

  // Callbacks should not be called.
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.success_events().size());

  EXPECT_EQ(0u, observer.rejected().size());

  // Confirm, that the request hasn't been removed, by rejecting it correctly.
  {
    const base::File::Error retry =
        request_manager_->RejectRequest(request_id, RequestValue(), error);
    EXPECT_EQ(base::File::FILE_OK, retry);
    EXPECT_EQ(1u, observer.rejected().size());
  }

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest, UniqueIds) {
  EventLogger logger;

  const int first_request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

  const int second_request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

  EXPECT_EQ(1, first_request_id);
  EXPECT_EQ(2, second_request_id);
}

TEST_F(FileSystemProviderRequestManagerTest, AbortOnDestroy) {
  EventLogger logger;
  RequestObserver observer;
  int request_id;

  {
    OperationRequestManager request_manager(
        profile_.get(), /*provider_id=*/std::string(),
        /*notification_manager=*/nullptr, /*timeout=*/base::Seconds(10));
    request_manager.AddObserver(&observer);

    request_id = request_manager.CreateRequest(
        kTestRequestType,
        base::WrapUnique<RequestManager::HandlerInterface>(
            new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));

    EXPECT_EQ(1, request_id);
    EXPECT_EQ(0u, logger.success_events().size());
    EXPECT_EQ(0u, logger.error_events().size());
    EXPECT_EQ(0u, logger.abort_events().size());

    ASSERT_EQ(1u, observer.created().size());
    EXPECT_EQ(request_id, observer.created()[0].request_id());
    EXPECT_EQ(kTestRequestType, observer.created()[0].type());

    ASSERT_EQ(1u, observer.executed().size());
    EXPECT_EQ(request_id, observer.executed()[0].request_id());

    EXPECT_EQ(0u, observer.fulfilled().size());
    EXPECT_EQ(0u, observer.rejected().size());
    EXPECT_EQ(0u, observer.destroyed().size());
    EXPECT_EQ(0u, observer.timed_out().size());

    // Do not remove the observer, to catch events while destructing.
  }

  // All active requests should be aborted in the destructor of RequestManager.
  ASSERT_EQ(1u, logger.error_events().size());
  EventLogger::ErrorEvent* event = logger.error_events()[0].get();
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, event->error());
  ASSERT_EQ(1u, logger.abort_events().size());
  EXPECT_EQ(request_id, logger.abort_events()[0]->request_id());

  EXPECT_EQ(0u, logger.success_events().size());

  EXPECT_EQ(0u, observer.fulfilled().size());
  EXPECT_EQ(0u, observer.timed_out().size());
  ASSERT_EQ(1u, observer.rejected().size());
  EXPECT_EQ(request_id, observer.rejected()[0].request_id());
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, observer.rejected()[0].error());
  ASSERT_EQ(1u, observer.destroyed().size());
  EXPECT_EQ(OperationCompletion::kAbortedInternally,
            observer.destroyed()[0].completion());
}

TEST_F(FileSystemProviderRequestManagerTest, AbortOnTimeout) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  request_manager_->SetTimeoutForTesting(base::Seconds(0));
  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));
  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, notification_manager_->size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  // Wait until the request is timed out.
  base::RunLoop().RunUntilIdle();

  // Abort the request.
  EXPECT_EQ(1u, notification_manager_->size());
  notification_manager_->Abort();
  EXPECT_EQ(0u, notification_manager_->size());

  ASSERT_EQ(1u, logger.error_events().size());
  EventLogger::ErrorEvent* event = logger.error_events()[0].get();
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, event->error());
  ASSERT_EQ(1u, logger.abort_events().size());
  EXPECT_EQ(request_id, logger.abort_events()[0]->request_id());

  ASSERT_EQ(1u, observer.rejected().size());
  EXPECT_EQ(request_id, observer.rejected()[0].request_id());
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, observer.rejected()[0].error());
  ASSERT_EQ(1u, observer.timed_out().size());
  EXPECT_EQ(request_id, observer.timed_out()[0].request_id());
  ASSERT_EQ(1u, observer.destroyed().size());
  EXPECT_EQ(request_id, observer.destroyed()[0].request_id());
  EXPECT_EQ(OperationCompletion::kAbortedFromNotification,
            observer.destroyed()[0].completion());

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest, ContinueOnTimeout) {
  EventLogger logger;
  RequestObserver observer;
  request_manager_->AddObserver(&observer);

  request_manager_->SetTimeoutForTesting(base::Seconds(0));
  const int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));
  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.abort_events().size());
  EXPECT_EQ(0u, notification_manager_->size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  // Wait until the request is timed out.
  base::RunLoop().RunUntilIdle();

  // Let the provider have more time by closing the notification.
  EXPECT_EQ(1u, notification_manager_->size());
  notification_manager_->Continue();
  EXPECT_EQ(0u, notification_manager_->size());

  // The request is still active.
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.abort_events().size());

  // Wait until the request is timed out again.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, notification_manager_->size());

  // Fulfill and check that operation completion mode changes.
  const base::File::Error result = request_manager_->FulfillRequest(
      request_id,
      RequestValue(RequestValue::CreateForTesting("i-like-vanilla")),
      /*has_more=*/false);
  EXPECT_EQ(base::File::FILE_OK, result);

  ASSERT_EQ(1u, observer.fulfilled().size());
  ASSERT_EQ(1u, observer.destroyed().size());
  EXPECT_EQ(OperationCompletion::kCompletedAfterWarning,
            observer.destroyed()[0].completion());

  request_manager_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderRequestManagerTest, NoNotificationWhileInteractive) {
  EventLogger logger;
  RequestObserver observer;
  base::ScopedObservation<RequestManager, RequestManager::Observer> observation(
      &observer);
  observation.Observe(request_manager_.get());

  request_manager_->StartUserInteraction();

  request_manager_->SetTimeoutForTesting(base::Seconds(0));
  int request_id = request_manager_->CreateRequest(
      kTestRequestType,
      base::WrapUnique<RequestManager::HandlerInterface>(
          new FakeHandler(logger.GetWeakPtr(), /*execute_reply=*/true)));
  EXPECT_EQ(1, request_id);
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.abort_events().size());
  EXPECT_EQ(0u, notification_manager_->size());

  ASSERT_EQ(1u, observer.created().size());
  EXPECT_EQ(request_id, observer.created()[0].request_id());
  EXPECT_EQ(kTestRequestType, observer.created()[0].type());

  ASSERT_EQ(1u, observer.executed().size());
  EXPECT_EQ(request_id, observer.executed()[0].request_id());

  // Wait until the request is timed out.
  TimeoutWaiter(request_manager_.get()).Wait();

  // Should not have shown a notification.
  EXPECT_EQ(0u, notification_manager_->size());

  // The request is still active.
  EXPECT_EQ(0u, logger.success_events().size());
  EXPECT_EQ(0u, logger.error_events().size());
  EXPECT_EQ(0u, logger.abort_events().size());

  // No longer interacting with the user.
  request_manager_->EndUserInteraction();

  // Wait until the request is timed out again.
  TimeoutWaiter(request_manager_.get()).Wait();

  // Shown a notification.
  EXPECT_EQ(1u, notification_manager_->size());
}

}  // namespace ash::file_system_provider
