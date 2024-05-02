// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert/client_cert_store_lacros.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/lacros/cert/cert_db_initializer.h"
#include "content/public/test/browser_task_environment.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::Invoke;
using testing::Pointer;

namespace {
class MockClientCertStore : public net::ClientCertStore {
 public:
  MOCK_METHOD(void,
              GetClientCerts,
              (scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
               ClientCertListCallback callback));
};

class MockCertDbInitializer : public CertDbInitializer {
 public:
  MOCK_METHOD(base::CallbackListSubscription,
              WaitUntilReady,
              (base::OnceClosure callback));
  MOCK_METHOD(NssCertDatabaseGetter,
              CreateNssCertDatabaseGetterForIOThread,
              ());
};

class ClientCertStoreLacrosTest : public ::testing::Test {
 public:
  ClientCertStoreLacrosTest() {
    cert_request_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
  }

  std::unique_ptr<MockClientCertStore> CreateMockStore(
      MockClientCertStore** non_owning_pointer) {
    auto store = std::make_unique<MockClientCertStore>();
    *non_owning_pointer = store.get();
    return store;
  }

  void FakeGetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertStoreLacros::ClientCertListCallback callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), net::ClientCertIdentityList()));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockCertDbInitializer cert_db_initializer_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_;
};

// Captures callback from `CertDbInitializer::WaitUntilReady(...)` and allows
// to imitate the "ready" notification by calling the `callback`.
struct DbInitCallbackHolder {
  base::CallbackListSubscription SaveCallback(base::OnceClosure cb) {
    callback = std::move(cb);
    return {};
  }
  base::OnceClosure callback;
};

// Provides callback for `ClientCertStore::GetClientCerts(...)` and allows to
// set expectations on it.
class GetCertsCallbackObserver {
 public:
  void GotClientCerts(net::ClientCertIdentityList) { loop_.Quit(); }

  auto GetCallback() {
    return base::BindOnce(&GetCertsCallbackObserver::GotClientCerts,
                          base::Unretained(this));
  }

  void WaitUntilGotCerts() { loop_.Run(); }

 private:
  base::RunLoop loop_;
};

// Test that if CertDbInitializing is not initially ready,
// ClientCertStoreLacros will wait for it.
TEST_F(ClientCertStoreLacrosTest, WaitsForInitialization) {
  DbInitCallbackHolder db_init_callback_holder;
  EXPECT_CALL(cert_db_initializer_, WaitUntilReady)
      .WillOnce(Invoke(&db_init_callback_holder,
                       &DbInitCallbackHolder::SaveCallback));

  // Create ClientCertStoreLacros.
  MockClientCertStore* underlying_store = nullptr;
  auto cert_store_lacros = std::make_unique<ClientCertStoreLacros>(
      nullptr, &cert_db_initializer_, CreateMockStore(&underlying_store));

  // Request client certs.
  GetCertsCallbackObserver get_certs_callback_observer;
  cert_store_lacros->GetClientCerts(cert_request_,
                                    get_certs_callback_observer.GetCallback());

  // The request should be forwarded to the underlying store, when executed.
  EXPECT_CALL(*underlying_store,
              GetClientCerts(Pointer(cert_request_.get()), /*callback=*/_))
      .WillOnce(Invoke(this, &ClientCertStoreLacrosTest::FakeGetClientCerts));

  // Imitate signal from cert_db_initializer_ that the initialization is done.
  // Even if it failed, the cert store should try to continue.
  std::move(db_init_callback_holder.callback).Run();
  get_certs_callback_observer.WaitUntilGotCerts();
}

// Test that if CertDbInitializing is initially ready, ClientCertStoreLacros
// will properly forward the `GetClientCerts` request to the underlying store.
TEST_F(ClientCertStoreLacrosTest, RunsImmediatelyIfReady) {
  DbInitCallbackHolder db_init_callback_holder;
  EXPECT_CALL(cert_db_initializer_, WaitUntilReady)
      .WillOnce(Invoke(&db_init_callback_holder,
                       &DbInitCallbackHolder::SaveCallback));

  // Create ClientCertStoreLacros.
  MockClientCertStore* underlying_store = nullptr;
  auto cert_store_lacros = std::make_unique<ClientCertStoreLacros>(
      nullptr, &cert_db_initializer_, CreateMockStore(&underlying_store));

  // Imitate signal from cert_db_initializer_ that the initialization is
  // done before calling `GetClientCerts`.
  std::move(db_init_callback_holder.callback).Run();

  EXPECT_CALL(*underlying_store,
              GetClientCerts(Pointer(cert_request_.get()), /*callback=*/_))
      .WillOnce(Invoke(this, &ClientCertStoreLacrosTest::FakeGetClientCerts));

  GetCertsCallbackObserver get_certs_callback_observer;
  // Because the cert db is already initialized, the callback should be called
  // immediately.
  cert_store_lacros->GetClientCerts(cert_request_,
                                    get_certs_callback_observer.GetCallback());
  get_certs_callback_observer.WaitUntilGotCerts();
}

// Test that ClientCertStoreLacros can queue multiple requests.
TEST_F(ClientCertStoreLacrosTest, QueueMultupleRequests) {
  DbInitCallbackHolder db_init_callback_holder;
  EXPECT_CALL(cert_db_initializer_, WaitUntilReady)
      .WillOnce(Invoke(&db_init_callback_holder,
                       &DbInitCallbackHolder::SaveCallback));

  // Create a lot of different requests.
  auto cert_request_1 = base::MakeRefCounted<net::SSLCertRequestInfo>();
  auto cert_request_2 = base::MakeRefCounted<net::SSLCertRequestInfo>();
  auto cert_request_3 = base::MakeRefCounted<net::SSLCertRequestInfo>();

  // Create ClientCertStoreLacros.
  MockClientCertStore* underlying_store = nullptr;
  auto cert_store_lacros = std::make_unique<ClientCertStoreLacros>(
      nullptr, &cert_db_initializer_, CreateMockStore(&underlying_store));

  // Request client certs for every cert request.

  GetCertsCallbackObserver get_certs_callback_observer_1;
  cert_store_lacros->GetClientCerts(
      cert_request_1, get_certs_callback_observer_1.GetCallback());
  EXPECT_CALL(*underlying_store,
              GetClientCerts(Pointer(cert_request_1.get()), /*callback=*/_))
      .WillOnce(Invoke(this, &ClientCertStoreLacrosTest::FakeGetClientCerts));

  GetCertsCallbackObserver get_certs_callback_observer_2;
  cert_store_lacros->GetClientCerts(
      cert_request_2, get_certs_callback_observer_2.GetCallback());
  EXPECT_CALL(*underlying_store,
              GetClientCerts(Pointer(cert_request_2.get()), /*callback=*/_))
      .WillOnce(Invoke(this, &ClientCertStoreLacrosTest::FakeGetClientCerts));

  GetCertsCallbackObserver get_certs_callback_observer_3;
  cert_store_lacros->GetClientCerts(
      cert_request_3, get_certs_callback_observer_3.GetCallback());
  EXPECT_CALL(*underlying_store,
              GetClientCerts(Pointer(cert_request_3.get()), /*callback=*/_))
      .WillOnce(Invoke(this, &ClientCertStoreLacrosTest::FakeGetClientCerts));

  // Imitate signal from cert_db_initializer_ that the initialization is done.
  std::move(db_init_callback_holder.callback).Run();

  get_certs_callback_observer_1.WaitUntilGotCerts();
  get_certs_callback_observer_2.WaitUntilGotCerts();
  get_certs_callback_observer_3.WaitUntilGotCerts();
}

// Test that ClientCertStoreLacros can be deleted from the last callback.
// (Deleting from a non-last one is prohibited by the API.)
TEST_F(ClientCertStoreLacrosTest, DeletedFromLastCallback) {
  DbInitCallbackHolder db_init_callback_holder;
  EXPECT_CALL(cert_db_initializer_, WaitUntilReady)
      .WillOnce(Invoke(&db_init_callback_holder,
                       &DbInitCallbackHolder::SaveCallback));

  // Create ClientCertStoreLacros.
  MockClientCertStore* underlying_store = nullptr;
  auto cert_store_lacros = std::make_unique<ClientCertStoreLacros>(
      nullptr, &cert_db_initializer_, CreateMockStore(&underlying_store));

  // Request client certs a couple of times.
  GetCertsCallbackObserver get_certs_callback_observer_1;
  cert_store_lacros->GetClientCerts(
      cert_request_, get_certs_callback_observer_1.GetCallback());

  GetCertsCallbackObserver get_certs_callback_observer_2;
  cert_store_lacros->GetClientCerts(
      cert_request_, get_certs_callback_observer_2.GetCallback());

  // Create a callback that will delete `cert_store_lacros` when executed and
  // pass it into the cert store. This code relies on the current implementation
  // of ClientCertStoreLacros and assumes that it will be executed last. If the
  // implementation changes, it is ok to move it around.
  GetCertsCallbackObserver get_certs_callback_observer_3;
  auto deleting_callback = [&](net::ClientCertIdentityList list) {
    get_certs_callback_observer_3.GotClientCerts(std::move(list));
    cert_store_lacros.reset();
  };
  cert_store_lacros->GetClientCerts(
      cert_request_, base::BindLambdaForTesting(std::move(deleting_callback)));

  // All 3 requests should be forwarded to the underlying store.
  EXPECT_CALL(*underlying_store,
              GetClientCerts((Pointer(cert_request_)), /*callback=*/_))
      .Times(3)
      .WillRepeatedly(
          Invoke(this, &ClientCertStoreLacrosTest::FakeGetClientCerts));

  // Imitate signal from cert_db_initializer_ that the initialization is
  // done.
  std::move(db_init_callback_holder.callback).Run();

  get_certs_callback_observer_1.WaitUntilGotCerts();
  get_certs_callback_observer_2.WaitUntilGotCerts();
  get_certs_callback_observer_3.WaitUntilGotCerts();

  // The third request should be able to delete the cert store without anything
  // crashing.
  EXPECT_FALSE(cert_store_lacros);
}

// Test that ClientCertStoreLacros can handle new cert requests during execution
// of another request (i.e. reentering).
TEST_F(ClientCertStoreLacrosTest, HandlesReentrancy) {
  DbInitCallbackHolder db_init_callback_holder;
  EXPECT_CALL(cert_db_initializer_, WaitUntilReady)
      .WillOnce(Invoke(&db_init_callback_holder,
                       &DbInitCallbackHolder::SaveCallback));

  // Create ClientCertStoreLacros.
  MockClientCertStore* underlying_store = nullptr;
  auto cert_store_lacros = std::make_unique<ClientCertStoreLacros>(
      nullptr, &cert_db_initializer_, CreateMockStore(&underlying_store));

  GetCertsCallbackObserver get_certs_callback_observer_1;
  GetCertsCallbackObserver get_certs_callback_observer_2;

  auto reentering_callback = [&](net::ClientCertIdentityList list) {
    get_certs_callback_observer_1.GotClientCerts(std::move(list));
    cert_store_lacros->GetClientCerts(
        cert_request_, get_certs_callback_observer_2.GetCallback());
  };

  // Request client certs with the reentering callback.
  cert_store_lacros->GetClientCerts(
      cert_request_,
      base::BindLambdaForTesting(std::move(reentering_callback)));

  EXPECT_CALL(*underlying_store,
              GetClientCerts(Pointer(cert_request_.get()), /*callback=*/_))
      .Times(2)
      .WillRepeatedly(
          Invoke(this, &ClientCertStoreLacrosTest::FakeGetClientCerts));

  // Imitate signal from cert_db_initializer_ that the initialization is done.
  std::move(db_init_callback_holder.callback).Run();

  // Verify that both callbacks are called.
  get_certs_callback_observer_1.WaitUntilGotCerts();
  get_certs_callback_observer_2.WaitUntilGotCerts();
}

}  // namespace
