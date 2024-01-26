// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service_manager.h"

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

using ClientId = PrintBackendServiceManager::ClientId;
using ClientsSet = PrintBackendServiceManager::ClientsSet;
using PrintClientsMap = PrintBackendServiceManager::PrintClientsMap;
using QueryWithUiClientsMap = PrintBackendServiceManager::QueryWithUiClientsMap;
using RemoteId = PrintBackendServiceManager::RemoteId;

namespace {

const RemoteId kRemoteIdEmpty{1};
const RemoteId kRemoteIdTestPrinter{2};

// ClientId values should not repeat for different types.
const ClientId kClientIdQuery1{1};
const ClientId kClientIdQuery2{2};
const ClientId kClientIdQueryWithUi1{5};
#if BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)
const ClientId kClientIdQueryWithUi2{6};
#endif
const ClientId kClientIdPrintDocument1{10};
const ClientId kClientIdPrintDocument2{11};
const ClientId kClientIdPrintDocument3{20};

const ClientsSet kTestQueryNoClients;
const ClientsSet kTestQueryWithOneClient{kClientIdQuery1};
const ClientsSet kTestQueryWithTwoClients{kClientIdQuery1, kClientIdQuery2};

const QueryWithUiClientsMap kTestQueryWithUiNoClients;
const QueryWithUiClientsMap kTestQueryWithUiOneClient{
    {kClientIdQueryWithUi1, kRemoteIdEmpty}};
#if BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)
const QueryWithUiClientsMap kTestQueryWithUiTwoClients{
    {kClientIdQueryWithUi1, kRemoteIdEmpty},
    {kClientIdQueryWithUi2, kRemoteIdEmpty}};
#endif

const PrintClientsMap kTestPrintDocumentNoClients;
const PrintClientsMap kTestPrintDocumentOnePrinterWithOneClient{
    {kRemoteIdEmpty, {kClientIdPrintDocument1}},
};
const PrintClientsMap kTestPrintDocumentOnePrinterWithTwoClients{
    {kRemoteIdEmpty, {kClientIdPrintDocument1, kClientIdPrintDocument2}},
};
const PrintClientsMap kTestPrintDocumentTwoPrintersWithOneClientEach{
    {kRemoteIdEmpty, {kClientIdPrintDocument1}},
    {kRemoteIdTestPrinter, {kClientIdPrintDocument3}},
};

constexpr std::optional<base::TimeDelta> kNoNewTimeoutNeeded;
constexpr std::optional<base::TimeDelta> kMaxTimeout = base::TimeDelta::Max();

}  // namespace

TEST(PrintBackendServiceManagerTest,
     IsIdleTimeoutUpdateNeededForRegisteredClient) {
  const struct TestData {
    ClientsSet query_clients;
    QueryWithUiClientsMap query_with_ui_client;
    PrintClientsMap print_document_clients;
    PrintBackendServiceManager::ClientType modified_client_type;
    std::optional<base::TimeDelta> new_timeout;
  } kTestData[] = {
    // == PrintBackendServiceManager::ClientType::kQuery

    // A single query client should yield a new clients-registered timeout.
    {
        kTestQueryWithOneClient,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQuery,
        PrintBackendServiceManager::kClientsRegisteredResetOnIdleTimeout,
    },
    // An extra query client should yield no new timeout needed.
    {
        kTestQueryWithTwoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQuery,
        kNoNewTimeoutNeeded,
    },
    // A single query client should yield no new timeout needed if a query
    // with UI is present.
    {
        kTestQueryWithOneClient,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQuery,
        kNoNewTimeoutNeeded,
    },
    // A single query client should yield no new timeout needed if a print
    // document is present.
    {
        kTestQueryWithOneClient,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kQuery,
        kNoNewTimeoutNeeded,
    },

    // == PrintBackendServiceManager::ClientType::kQueryWithUi

    // A lone query with UI client should yield a new maximum timeout.
    {
        kTestQueryNoClients,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        kMaxTimeout,
    },
    // A new query with UI client with existing regular queries should yield a
    // new maximum timeout.
    {
        kTestQueryWithTwoClients,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        kMaxTimeout,
    },
#if BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)
    // A new query with UI client with an existing query with UI client
    // should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiTwoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        kNoNewTimeoutNeeded,
    },
#endif
    // A new query with UI client with an existing printing client should
    // yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        kNoNewTimeoutNeeded,
    },

    // == PrintBackendServiceManager::ClientType::kPrintDocument

    // A lone print document client should yield a new maximum timeout.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        kMaxTimeout,
    },
    // An extra print document client should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentOnePrinterWithTwoClients,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        kNoNewTimeoutNeeded,
    },
    // A new print document client with existing regular queries should yield
    // a new maximum timeout.
    {
        kTestQueryWithTwoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        kMaxTimeout,
    },
    // A new print document client with existing query with UI should yield no
    // new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        kNoNewTimeoutNeeded,
    },
    // A new print document client with existing print document for alternate
    // remote ID should yield a new maximum timeout.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentTwoPrintersWithOneClientEach,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        kMaxTimeout,
    },
  };

  for (const auto& test_data : kTestData) {
    PrintBackendServiceManager::ResetForTesting();
    PrintBackendServiceManager::GetInstance().SetClientsForTesting(
        test_data.query_clients, test_data.query_with_ui_client,
        test_data.print_document_clients);

    std::optional<base::TimeDelta> new_timeout =
        PrintBackendServiceManager::GetInstance()
            .DetermineIdleTimeoutUpdateOnRegisteredClient(
                test_data.modified_client_type, kRemoteIdEmpty);
    EXPECT_EQ(new_timeout, test_data.new_timeout);
  }
}

TEST(PrintBackendServiceManagerTest,
     IsIdleTimeoutUpdateNeededForUnregisteredClient) {
  const struct TestData {
    ClientsSet query_clients;
    QueryWithUiClientsMap query_with_ui_client;
    PrintClientsMap print_document_clients;
    PrintBackendServiceManager::ClientType modified_client_type;
    std::optional<base::TimeDelta> new_timeout;
  } kTestData[] = {
    // == PrintBackendServiceManager::ClientType::kQuery

    // No remaining clients should yield a new no-clients-registered timeout.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQuery,
        PrintBackendServiceManager::kNoClientsRegisteredResetOnIdleTimeout,
    },
    // Any remaining query client should yield no new timeout needed.
    {
        kTestQueryWithOneClient,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQuery,
        kNoNewTimeoutNeeded,
    },
    // A query with UI client should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQuery,
        kNoNewTimeoutNeeded,
    },
    // Any print document client should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kQuery,
        kNoNewTimeoutNeeded,
    },

    // == PrintBackendServiceManager::ClientType::kQueryWithUi

    // No remaining clients should yield a new no-clients-registered timeout.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        PrintBackendServiceManager::kNoClientsRegisteredResetOnIdleTimeout,
    },
    // Any remaining query client should yield a new clients-registered
    // timeout.
    {
        kTestQueryWithOneClient,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        PrintBackendServiceManager::kClientsRegisteredResetOnIdleTimeout,
    },
#if BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)
    // Any remaining query with UI client should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        kNoNewTimeoutNeeded,
    },
#endif
    // Any print document client should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kQueryWithUi,
        kNoNewTimeoutNeeded,
    },

    // == PrintBackendServiceManager::ClientType::kPrintDocument

    // No remaining clients should yield a new no-clients-registered timeout.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        PrintBackendServiceManager::kNoClientsRegisteredResetOnIdleTimeout,
    },
    // Any remaining print document client should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentOnePrinterWithOneClient,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        kNoNewTimeoutNeeded,
    },
    // Any remaining query client should yield a new clients-registered
    // timeout.
    {
        kTestQueryWithOneClient,
        kTestQueryWithUiNoClients,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        PrintBackendServiceManager::kClientsRegisteredResetOnIdleTimeout,
    },
    // A remaining query with UI should yield no new timeout needed.
    {
        kTestQueryNoClients,
        kTestQueryWithUiOneClient,
        kTestPrintDocumentNoClients,
        PrintBackendServiceManager::ClientType::kPrintDocument,
        kNoNewTimeoutNeeded,
    },
  };

  for (const auto& test_data : kTestData) {
    PrintBackendServiceManager::ResetForTesting();
    PrintBackendServiceManager::GetInstance().SetClientsForTesting(
        test_data.query_clients, test_data.query_with_ui_client,
        test_data.print_document_clients);

    std::optional<base::TimeDelta> new_timeout =
        PrintBackendServiceManager::GetInstance()
            .DetermineIdleTimeoutUpdateOnUnregisteredClient(
                test_data.modified_client_type, kRemoteIdEmpty);
    EXPECT_EQ(new_timeout, test_data.new_timeout);
  }
}

}  // namespace printing
