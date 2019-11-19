// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"

class Profile;

namespace chromeos {
namespace file_system_provider {

// Request type, passed to RequestManager::CreateRequest. For logging purposes.
enum RequestType {
  REQUEST_UNMOUNT,
  GET_METADATA,
  GET_ACTIONS,
  EXECUTE_ACTION,
  READ_DIRECTORY,
  OPEN_FILE,
  CLOSE_FILE,
  READ_FILE,
  CREATE_DIRECTORY,
  DELETE_ENTRY,
  CREATE_FILE,
  COPY_ENTRY,
  MOVE_ENTRY,
  TRUNCATE,
  WRITE_FILE,
  ABORT,
  ADD_WATCHER,
  REMOVE_WATCHER,
  CONFIGURE,
  TESTING
};

// Manages requests between the service, async utils and the providing
// extension or native provider.
class RequestManager {
 public:
  // Handles requests. Each request implementation must implement
  // this interface.
  class HandlerInterface {
   public:
    virtual ~HandlerInterface() {}

    // Called when the request is created. Executes the request implementation.
    // Returns false in case of a execution failure.
    virtual bool Execute(int request_id) = 0;

    // Success callback invoked by the provider in response to
    // Execute(). It may be called more than once, until |has_more| is set to
    // false.
    virtual void OnSuccess(int request_id,
                           std::unique_ptr<RequestValue> result,
                           bool has_more) = 0;

    // Error callback invoked by the providing extension in response to
    // Execute(). It can be called at most once. It can be also called if the
    // request is aborted due to a timeout.
    virtual void OnError(int request_id,
                         std::unique_ptr<RequestValue> result,
                         base::File::Error error) = 0;
  };

  // Observes activities in the request manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the request is created.
    virtual void OnRequestCreated(int request_id, RequestType type) = 0;

    // Called when the request is destroyed.
    virtual void OnRequestDestroyed(int request_id) = 0;

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

    // Called when the request is timeouted.
    virtual void OnRequestTimeouted(int request_id) = 0;
  };

  // Creates a request manager for |profile| and |provider_id|. Note, that
  // there may be multiple instances of request managers per provider.
  RequestManager(Profile* profile,
                 const std::string& provider_id,
                 NotificationManagerInterface* notification_manager);
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
                                   std::unique_ptr<RequestValue> response,
                                   bool has_more);

  // Handles error response for the |request_id|. If handling the error
  // succeeds, theen returns base::File::FILE_OK. Otherwise returns an error
  // code. Always disposes the request. |response| must not be NULL.
  base::File::Error RejectRequest(int request_id,
                                  std::unique_ptr<RequestValue> response,
                                  base::File::Error error);

  // Sets a custom timeout for tests. The new timeout value will be applied to
  // new requests
  void SetTimeoutForTesting(const base::TimeDelta& timeout);

  // Gets list of active request ids.
  std::vector<int> GetActiveRequestIds() const;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  struct Request {
    Request();
    ~Request();

    // Timer for discarding the request during a timeout.
    base::OneShotTimer timeout_timer;

    // Handler tied to this request.
    std::unique_ptr<HandlerInterface> handler;

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  // Destroys the request with the passed |request_id|.
  void DestroyRequest(int request_id);

  // Called when a request with |request_id| timeouts.
  void OnRequestTimeout(int request_id);

  // Called when an user either aborts the unresponsive request or lets it
  // continue.
  void OnUnresponsiveNotificationResult(
      int request_id,
      NotificationManagerInterface::NotificationResult result);

  // Resets the timeout timer for the specified request.
  void ResetTimer(int request_id);

  // Checks whether there is an ongoing interaction between the provider
  // and user.
  bool IsInteractingWithUser() const;

  Profile* profile_;  // Not owned.
  std::string provider_id_;
  std::map<int, std::unique_ptr<Request>> requests_;
  NotificationManagerInterface* notification_manager_;  // Not owned.
  int next_id_;
  base::TimeDelta timeout_;
  base::ObserverList<Observer>::Unchecked observers_;
  base::WeakPtrFactory<RequestManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RequestManager);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
