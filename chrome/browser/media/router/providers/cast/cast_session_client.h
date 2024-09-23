// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace media_router {

// Represents a Cast SDK client connection to a Cast session. This class
// contains PresentationConnection Mojo pipes to send and receive messages
// from/to the corresponding SDK client hosted in a presentation controlling
// frame in Blink.
class CastSessionClient {
 public:
  CastSessionClient(const std::string& client_id,
                    const url::Origin& origin,
                    content::FrameTreeNodeId frame_tree_node_id);
  CastSessionClient(const CastSessionClient&) = delete;
  CastSessionClient& operator=(const CastSessionClient&) = delete;
  virtual ~CastSessionClient();

  const std::string& client_id() const { return client_id_; }
  const std::optional<std::string>& session_id() const { return session_id_; }
  const url::Origin& origin() const { return origin_; }
  content::FrameTreeNodeId frame_tree_node_id() const {
    return frame_tree_node_id_;
  }

  // Initializes the PresentationConnection Mojo message pipes and returns the
  // handles of the two pipes to be held by Blink. Also transitions the
  // connection state to CONNECTED. This method can only be called once, and
  // must be called before |SendMessageToClient()|.
  virtual mojom::RoutePresentationConnectionPtr Init() = 0;

  // Sends |message| to the Cast SDK client in Blink.
  virtual void SendMessageToClient(
      blink::mojom::PresentationConnectionMessagePtr message) = 0;

  // Sends a media message to the client.  If |request_id| is given, it
  // is used to look up the sequence number of a previous request, which is
  // included in the outgoing message.
  virtual void SendMediaMessageToClient(const base::Value::Dict& payload,
                                        std::optional<int> request_id) = 0;

  // Changes the PresentationConnection state to CLOSED/TERMINATED and resets
  // PresentationConnection message pipes.
  virtual void CloseConnection(
      blink::mojom::PresentationConnectionCloseReason close_reason) = 0;
  virtual void TerminateConnection() = 0;

  // Tests whether the specified origin and FrameTreeNode ID match this
  // session's origin and FrameTreeNode ID to the extent required by this
  // sesssion's auto-join policy. Depending on the value of |auto_join_policy_|,
  // |origin|, |frame_tree_node_id_|, or both may be ignored.
  //
  // TODO(crbug.com/1291742): It appears the purpose of this method is to detect
  // whether this session was created by an auto-join request.  It might make
  // more sense to record at session creation time whether a particular session
  // was created by an auto-join request, in which case this method would no
  // longer be needed.
  virtual bool MatchesAutoJoinPolicy(
      url::Origin origin,
      content::FrameTreeNodeId frame_tree_node_id) const = 0;

  virtual void SendErrorCodeToClient(
      int sequence_number,
      CastInternalMessage::ErrorCode error_code,
      std::optional<std::string> description) = 0;

  // NOTE: This is current only called from SendErrorCodeToClient, but based on
  // the old code this method based on, it seems likely it will have other
  // callers once error handling for the Cast MRP is more fleshed out.
  virtual void SendErrorToClient(int sequence_number,
                                 base::Value::Dict error) = 0;

 private:
  std::string client_id_;
  std::optional<std::string> session_id_;

  // The origin and FrameTreeNode ID parameters originally passed to the
  // CreateRoute method of the MediaRouteProvider Mojo interface.
  url::Origin origin_;
  content::FrameTreeNodeId frame_tree_node_id_;
};

class CastSessionClientFactoryForTest {
 public:
  virtual std::unique_ptr<CastSessionClient> MakeClientForTest(
      const std::string& client_id,
      const url::Origin& origin,
      content::FrameTreeNodeId frame_tree_node_id) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_SESSION_CLIENT_H_
