// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/glic_private/glic_private_api_test_base.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

namespace extensions {

const char kGlicPrivateTestExtensionId[] = "oljbkhokcbpaencibijkoolhipplkeoc";

GlicPrivateApiTestBase::GlicPrivateApiTestBase() {
  ComponentLoader::EnableBackgroundExtensionsForTesting();
  UseHttpsTestServer();

  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.dns_names = {"gemini.google.com", "example.com"};
  embedded_test_server()->SetSSLConfig(cert_config);
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  EXPECT_TRUE(embedded_test_server()->Start());
}

GlicPrivateApiTestBase::~GlicPrivateApiTestBase() = default;

void GlicPrivateApiTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // Add a host resolver rule to map all outgoing requests to the test server.
  // This allows us to use "real" hostnames and standard ports in URLs (i.e.,
  // without having to inject the port number into all URLs).
  int port = embedded_test_server()->port();
  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::StringPrintf("MAP * 127.0.0.1:%d", port));
  command_line->AppendSwitchASCII(extensions::switches::kAllowlistedExtensionID,
                                  kGlicPrivateTestExtensionId);

  ExtensionApiTest::SetUpCommandLine(command_line);
}

void GlicPrivateApiTestBase::SetupIdentityAndCapabilities() {
  Profile* test_profile = profile();
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(test_profile), "test@example.com",
      signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_gemini_in_chrome(true);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(
      IdentityManagerFactory::GetForProfile(test_profile), account_info);

  signin::SetAutomaticIssueOfAccessTokens(
      IdentityManagerFactory::GetForProfile(test_profile), true);
}

// static
std::unique_ptr<content::URLLoaderInterceptor>
GlicPrivateApiTestBase::CreateMockPromptResponseInterceptor(
    const std::string& prompt_data) {
  return std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
      [](const std::string& prompt,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec().find(
                extensions_features::kProdPromptEndpointUrlParam.Get()) !=
            std::string::npos) {
          std::string request_body;
          if (params->url_request.request_body) {
            for (const auto& element :
                 *params->url_request.request_body->elements()) {
              if (element.type() == network::DataElement::Tag::kBytes) {
                const auto& bytes = element.As<network::DataElementBytes>();
                request_body.append(
                    reinterpret_cast<const char*>(bytes.bytes().data()),
                    bytes.bytes().size());
              }
            }
          }

          if (request_body.find("http_error") != std::string::npos) {
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 500 Internal Server Error\nContent-type: "
                "text/plain\n\n",
                "Internal Error", params->client.get());
            return true;
          }

          if (request_body.find("parse_error") != std::string::npos) {
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\nContent-type: application/json\n\n",
                "malformed response", params->client.get());
            return true;
          }

          base::Value response_dict(base::Value::Type::DICT);
          if (request_body.find("missing_prompt") == std::string::npos) {
            response_dict.GetDict().Set("prompt", prompt);
          }

          std::string response_str;
          base::JSONWriter::Write(response_dict, &response_str);
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-type: application/json\n\n",
              response_str, params->client.get());
          return true;
        }
        return false;
      },
      prompt_data));
}

}  // namespace extensions
