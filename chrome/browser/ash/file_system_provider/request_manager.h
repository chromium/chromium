// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"

class Profile;

namespace ash::file_system_provider {

// Request type, passed to RequestManager::CreateRequest. For logging purposes.
enum class RequestType {
  kAbort = 0,
  kAddWatcher = 1,
  kCloseFile = 2,
  kConfigure = 3,
  kCopyEntry = 4,
  kCreateDirectory = 5,
  kCreateFile = 6,
  kDeleteEntry = 7,
  kExecuteAction = 8,
  kGetActions = 9,
  kGetMetadata = 10,
  kMount = 11,
  kMoveEntry = 12,
  kOpenFile = 13,
  kReadDirectory = 14,
  kReadFile = 15,
  kRemoveWatcher = 16,
  kTruncate = 17,
  kUnmount = 18,
  kWriteFile = 19,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OperationCompletion {
  kCompletedNormally = 0,
  kCompletedAfterWarning = 1,
  kAbortedFromNotification = 2,
  kAbortedInternally = 3,
  kMaxValue = kAbortedInternally,
};

// Manages requests between the service, async utils and the providing
// extension or native provider.
class RequestManager {
 public:
  // Handles requests. Each request implementation must implement
  // this interface.
  class HandlerInterface {
   public:
    virtual ~HandlerInterface() = default;

    // Called when the request is created. Executes the request implementation.
    // Returns false in case of a execution failure.
    virtual bool Execute(int request_id) = 0;

    // Success callback invoked by the provider in response to
    // Execute(). It may be called more than once, until |has_more| is set to
    // false.
    virtual void OnSuccess(int request_id,
                           const RequestValue& result,
                           bool has_more) = 0;

    // Error callback invoked by the providing extension in response to
    // Execute(). It can be called at most once. It can be also called if the
    // request is aborted due to a timeout.
    virtual void OnError(int request_id,
                         const RequestValue& result,
                         base::File::Error error) = 0;

    // Called when the request is aborted due to timeout, before |OnError| is
    // called.
    virtual void OnAbort(int request_id) = 0;
  };

  // Observes activities in the request manager.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the request is created.
    virtual void OnRequestCreated(int request_id, RequestType type) = 0;

    // Called when the request is destroyed.
    virtual void OnRequestDestroyed(int request_id,
                                    OperationCompletion completion) = 0;

    // Called when the request is executed.
    virtual void OnRequestExecuted(int request_id) = 0;

    // Called when the request is fulfilled with a success.
    virtual void OnRequestFulfilled(int request_id,
                                    const RequestValue& result,
                                    bool has_more) = 0;

    // Called when the request is rejected with an error.
    virtual void OnRequestRejected(int request_id,
                                   const RequestValue& result,
                                   base::File::Error error) = 0;

    // Called when the request is timed out.
    virtual void OnRequestTimedOut(int request_id) = 0;
  };

  RequestManager(Profile* profile,
                 NotificationManagerInterface* notification_manager,
                 base::TimeDelta timeout);

  RequestManager(const RequestManager&) = delete;
  RequestManager& operator=(const RequestManager&) = delete;

  virtual ~RequestManager();

  // Creates a request and returns its request id (greater than 0). Returns 0 in
  // case of an error (eg. too many requests). The |type| argument indicates
  // what kind of request it is.
  int CreateRequest(RequestType type,
                    std::unique_ptr<HandlerInterface> handler);

  // Handles successful response for the |request_id|. If |has_more| is false,
  // then the request is disposed, after handling the |response|. On success,
  // returns base::File::FILE_OK. Otherwise returns an error code. |response|
  // must not be NULL.
  base::File::Error FulfillRequest(int request_id,
                                   const RequestValue& response,
                                   bool has_more);

  // Handles error response for the |request_id|. If handling the error
  // succeeds, theen returns base::File::FILE_OK. Otherwise returns an error
  // code. Always disposes the request. |response| must not be NULL.
  base::File::Error RejectRequest(int request_id,
                                  const RequestValue& response,
                                  base::File::Error error);

  // Sets a custom timeout for tests. The new timeout value will be applied to
  // new requests
  void SetTimeoutForTesting(const base::TimeDelta& timeout);

  // Gets list of active request ids.
  std::vector<int> GetActiveRequestIds() const;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Destroys the request with the passed |request_id|.
  void DestroyRequest(int request_id, OperationCompletion completion);

 protected:
  struct Request {
    Request();

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    ~Request();

    // Timer for discarding the request during a timeout.
    base::OneShotTimer timeout_timer;

    // Handler tied to this request.
    std::unique_ptr<HandlerInterface> handler;

    // Indicates if this operation timed out and a warning has been shown to the
    // user.
    bool shown_unresponsive_notification = false;
  };

  // Called when a request with |request_id| timeouts.
  virtual void OnRequestTimeout(int request_id);

  // Called when an user either aborts the unresponsive request or lets it
  // continue.
  void OnUnresponsiveNotificationResult(
      int request_id,
      NotificationManagerInterface::NotificationResult result);

  // Resets the timeout timer for the specified request.
  void ResetTimer(int request_id);

  // Reject a request specifying how it was completed.
  base::File::Error RejectRequestInternal(int request_id,
                                          const RequestValue& response,
                                          base::File::Error error,
                                          OperationCompletion completion);

  raw_ptr<Profile> profile_;  // Not owned.
  std::map<int, std::unique_ptr<Request>> requests_;
  raw_ptr<NotificationManagerInterface, DanglingUntriaged>
      notification_manager_;  // Not owned.
  int next_id_;
  base::TimeDelta timeout_;
  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_;
  base::WeakPtrFactory<RequestManager> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
