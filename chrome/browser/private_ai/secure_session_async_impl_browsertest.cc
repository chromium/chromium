// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/secure_session_async_impl.h"

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/private_ai/crypto/constants.h"
#include "components/private_ai/crypto/test_server_secure_session.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace private_ai {

namespace {

HandshakeMessage ConvertToHandshakeMessage(
    const oak::session::v1::HandshakeRequest& response) {
  CHECK(response.has_noise_handshake_message());

  const auto noise_msg = response.noise_handshake_message();

  CHECK_EQ(noise_msg.ephemeral_public_key().size(), kP256X962Length);

  std::array<uint8_t, kP256X962Length> ephemeral_public_key;
  base::span(ephemeral_public_key)
      .copy_from(base::as_byte_span(noise_msg.ephemeral_public_key()));

  HandshakeMessage output(
      ephemeral_public_key,
      base::ToVector(base::as_byte_span(noise_msg.ciphertext())));

  return output;
}

oak::session::v1::HandshakeResponse ConvertToResponseProto(
    const HandshakeMessage& input) {
  oak::session::v1::HandshakeResponse output;
  output.mutable_noise_handshake_message()->set_ephemeral_public_key(
      input.ephemeral_public_key.data(), input.ephemeral_public_key.size());
  output.mutable_noise_handshake_message()->set_ciphertext(
      input.ciphertext.data(), input.ciphertext.size());
  return output;
}

oak::session::v1::EncryptedMessage ConvertToEncryptedMessage(
    const std::vector<uint8_t>& encrypted_data) {
  oak::session::v1::EncryptedMessage encrypted_message;
  encrypted_message.set_ciphertext(encrypted_data.data(),
                                   encrypted_data.size());
  return encrypted_message;
}

std::vector<uint8_t> ConvertToBytes(
    const oak::session::v1::EncryptedMessage& encrypted_msg) {
  return std::vector<uint8_t>(encrypted_msg.ciphertext().begin(),
                              encrypted_msg.ciphertext().end());
}

class SecureSessionAsyncImplBrowserTest : public InProcessBrowserTest {
 public:
  // content::BrowserTestBase override:
  void SetUpOnMainThread() override {
    // `client_session_` spawns a separate process for crypto operations,
    // therefore it has to be initialized after browser init.
    client_session_ = std::make_unique<SecureSessionAsyncImpl>();
  }

 protected:
  void PerformValidHandshake(TestServerSecureSession& server_session) {
    auto client_handshake_request = [&]() {
      base::test::TestFuture<std::optional<oak::session::v1::HandshakeRequest>>
          future;
      client_session_->GetHandshakeMessage(future.GetCallback());

      auto result = future.Get();
      CHECK(result.has_value());

      return result.value();
    }();

    auto server_handshake_response = server_session.ProcessHandshake(
        ConvertToHandshakeMessage(client_handshake_request));
    ASSERT_TRUE(server_handshake_response.has_value());

    {
      base::test::TestFuture<bool> future;
      client_session_->ProcessHandshakeResponse(
          ConvertToResponseProto(server_handshake_response.value()),
          future.GetCallback());
      ASSERT_TRUE(future.Get());
    }
  }

 protected:
  std::unique_ptr<SecureSessionAsyncImpl> client_session_;
};

// End-to-end test of the handshake and encryption/decryption in both
// directions.
IN_PROC_BROWSER_TEST_F(SecureSessionAsyncImplBrowserTest,
                       HandshakeAndEncryptDecryptSucceeds) {
  TestServerSecureSession server_session;
  PerformValidHandshake(server_session);

  // Test encryption and decryption from client to server.
  const Request client_plaintext = {1, 2, 3};
  auto encrypted_from_client = [&]() {
    base::test::TestFuture<std::optional<oak::session::v1::EncryptedMessage>>
        future;
    client_session_->Encrypt(client_plaintext, future.GetCallback());
    return future.Get();
  }();
  ASSERT_TRUE(encrypted_from_client.has_value());

  auto decrypted_by_server =
      server_session.Decrypt(ConvertToBytes(encrypted_from_client.value()));
  ASSERT_TRUE(decrypted_by_server.has_value());
  EXPECT_EQ(client_plaintext, decrypted_by_server.value());

  // Test encryption and decryption from server to client.
  const Request server_plaintext = {4, 5, 6};
  auto encrypted_from_server = server_session.Encrypt(server_plaintext);
  ASSERT_TRUE(encrypted_from_server.has_value());

  {
    base::test::TestFuture<const std::optional<Response>&> future;
    client_session_->Decrypt(
        ConvertToEncryptedMessage(encrypted_from_server.value()),
        future.GetCallback());
    auto decrypted_by_client = future.Get();

    ASSERT_TRUE(decrypted_by_client.has_value());
    EXPECT_EQ(server_plaintext, decrypted_by_client.value());
  }
}

IN_PROC_BROWSER_TEST_F(SecureSessionAsyncImplBrowserTest,
                       GetHandshakeMessageSucceeds) {
  base::test::TestFuture<std::optional<oak::session::v1::HandshakeRequest>>
      future;
  client_session_->GetHandshakeMessage(future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());

  auto request = result.value();
  ASSERT_TRUE(request.has_noise_handshake_message());

  const auto& noise_msg = request.noise_handshake_message();
  EXPECT_EQ(noise_msg.ephemeral_public_key().size(), kP256X962Length);
  EXPECT_FALSE(noise_msg.ciphertext().empty());
}

IN_PROC_BROWSER_TEST_F(SecureSessionAsyncImplBrowserTest,
                       ProcessHandshakeResponseInvalidPeerKey) {
  // Though the result is not used, it's important to call GetHandshakeMessage()
  // before ProcessHandshakeResponse().
  {
    base::test::TestFuture<std::optional<oak::session::v1::HandshakeRequest>>
        future;
    client_session_->GetHandshakeMessage(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  oak::session::v1::HandshakeResponse response;
  auto* noise_msg = response.mutable_noise_handshake_message();
  // Malform the key by providing an incorrect size.
  noise_msg->set_ephemeral_public_key("invalid key", 11);
  noise_msg->set_ciphertext("some ciphertext");

  {
    base::test::TestFuture<bool> future;
    client_session_->ProcessHandshakeResponse(response, future.GetCallback());
    EXPECT_FALSE(future.Get());
  }
}

IN_PROC_BROWSER_TEST_F(SecureSessionAsyncImplBrowserTest,
                       ProcessHandshakeResponseInvalidCiphertext) {
  // Though the result is not used, it's important to call GetHandshakeMessage()
  // before ProcessHandshakeResponse().
  {
    base::test::TestFuture<std::optional<oak::session::v1::HandshakeRequest>>
        future;
    client_session_->GetHandshakeMessage(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Create a valid server response, but then corrupt the ciphertext.
  oak::session::v1::HandshakeResponse server_handshake_response;
  auto* server_noise_msg =
      server_handshake_response.mutable_noise_handshake_message();

  uint8_t server_e_pub_bytes[kP256X962Length] = {0};  // Test key
  server_noise_msg->set_ephemeral_public_key(server_e_pub_bytes,
                                             sizeof(server_e_pub_bytes));
  server_noise_msg->set_ciphertext("corrupted ciphertext");

  {
    base::test::TestFuture<bool> future;
    client_session_->ProcessHandshakeResponse(server_handshake_response,
                                              future.GetCallback());
    EXPECT_FALSE(future.Get());
  }
}

// Tests that ProcessHandshakeResponse fails if called before
// GetHandshakeMessage.
IN_PROC_BROWSER_TEST_F(SecureSessionAsyncImplBrowserTest,
                       ProcessHandshakeResponseWithoutHandshake) {
  oak::session::v1::HandshakeResponse response;

  base::test::TestFuture<bool> future;
  client_session_->ProcessHandshakeResponse(response, future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// Tests that the handshake fails if the server's response includes a payload,
// which is not allowed in the NN handshake pattern.
IN_PROC_BROWSER_TEST_F(SecureSessionAsyncImplBrowserTest,
                       ProcessHandshakeResponseNonEmptyPlaintext) {
  auto client_handshake_request = [&]() {
    base::test::TestFuture<std::optional<oak::session::v1::HandshakeRequest>>
        future;
    client_session_->GetHandshakeMessage(future.GetCallback());

    auto result = future.Get();
    CHECK(result.has_value());

    return result.value();
  }();

  TestServerSecureSession server_session;
  // Generate a server response with a non-empty payload, which is invalid for
  // the NN handshake pattern.
  auto server_handshake_response = server_session.ProcessHandshake(
      ConvertToHandshakeMessage(client_handshake_request), {1, 2, 3});
  ASSERT_TRUE(server_handshake_response.has_value());

  // The client should reject the response because the decrypted payload is not
  // empty.
  {
    base::test::TestFuture<bool> future;
    client_session_->ProcessHandshakeResponse(
        ConvertToResponseProto(server_handshake_response.value()),
        future.GetCallback());
    EXPECT_FALSE(future.Get());
  }
}

}  // namespace

}  // namespace private_ai
