// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace {
AppShimHostBootstrap::Client* g_client = nullptr;
}  // namespace

// static
void AppShimHostBootstrap::SetClient(Client* client) {
  g_client = client;
}

// static
void AppShimHostBootstrap::CreateForChannelAndPeerID(
    mojo::PlatformChannelEndpoint endpoint,
    base::ProcessId peer_pid) {
  // AppShimHostBootstrap is initially owned by itself until it receives a
  // OnShimConnected message or a channel error. In OnShimConnected, ownership
  // is transferred to a unique_ptr.
  DCHECK(endpoint.platform_handle().is_mach_send());
  (new AppShimHostBootstrap(peer_pid))->ServeChannel(std::move(endpoint));
}

AppShimHostBootstrap::AppShimHostBootstrap(base::ProcessId peer_pid)
    : pid_(peer_pid) {}

AppShimHostBootstrap::~AppShimHostBootstrap() {
  DCHECK(!shim_connected_callback_);
}

void AppShimHostBootstrap::ServeChannel(
    mojo::PlatformChannelEndpoint endpoint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  mojo::ScopedMessagePipeHandle message_pipe =
      bootstrap_mojo_connection_.Connect(std::move(endpoint));
  host_bootstrap_receiver_.Bind(
      mojo::PendingReceiver<chrome::mojom::AppShimHostBootstrap>(
          std::move(message_pipe)));
  host_bootstrap_receiver_.set_disconnect_with_reason_handler(base::BindOnce(
      &AppShimHostBootstrap::ChannelError, base::Unretained(this)));
}

void AppShimHostBootstrap::ChannelError(uint32_t custom_reason,
                                        const std::string& description) {
  // Once |this| has received a OnShimConnected message, it is owned by a
  // unique_ptr (not the channel anymore).
  if (app_shim_info_)
    return;
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  delete this;
}

mojo::PendingReceiver<chrome::mojom::AppShimHost>
AppShimHostBootstrap::GetAppShimHostReceiver() {
  return std::move(app_shim_host_receiver_);
}

const std::string& AppShimHostBootstrap::GetAppId() const {
  return app_shim_info_->app_id;
}

const GURL& AppShimHostBootstrap::GetAppURL() {
  return app_shim_info_->app_url;
}

const base::FilePath& AppShimHostBootstrap::GetProfilePath() {
  return app_shim_info_->profile_path;
}

apps::AppShimLaunchType AppShimHostBootstrap::GetLaunchType() const {
  return app_shim_info_->launch_type;
}

const std::vector<base::FilePath>& AppShimHostBootstrap::GetLaunchFiles()
    const {
  return app_shim_info_->files;
}

bool AppShimHostBootstrap::IsMultiProfile() const {
  // PWAs and bookmark apps are multi-profile capable.
  return base::FeatureList::IsEnabled(features::kAppShimMultiProfile) &&
         app_shim_info_->app_url.is_valid();
}

void AppShimHostBootstrap::OnShimConnected(
    mojo::PendingReceiver<chrome::mojom::AppShimHost> app_shim_host_receiver,
    chrome::mojom::AppShimInfoPtr app_shim_info,
    OnShimConnectedCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!app_shim_info_);
  // Only one app launch message per channel.
  if (app_shim_info_)
    return;

  app_shim_host_receiver_ = std::move(app_shim_host_receiver);
  app_shim_info_ = std::move(app_shim_info);
  shim_connected_callback_ = std::move(callback);

  // Transfer ownership to a unique_ptr and mark that OnShimConnected has been
  // received. Note that after this point, a channel error will no longer
  // cause |this| to be deleted.
  std::unique_ptr<AppShimHostBootstrap> deleter(this);

  // |g_client| takes ownership of |this| now.
  if (g_client)
    g_client->OnShimProcessConnected(std::move(deleter));

  // |g_client| can only be nullptr after AppShimListener is destroyed. Since
  // this only happens at shutdown, do nothing here.
}

void AppShimHostBootstrap::OnConnectedToHost(
    mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
  std::move(shim_connected_callback_)
      .Run(apps::APP_SHIM_LAUNCH_SUCCESS, std::move(app_shim_receiver));
}

void AppShimHostBootstrap::OnFailedToConnectToHost(
    apps::AppShimLaunchResult result) {
  // Because there will be users of the AppShim interface in failure, just
  // return a dummy receiver.
  mojo::Remote<chrome::mojom::AppShim> dummy_remote;
  std::move(shim_connected_callback_)
      .Run(result, dummy_remote.BindNewPipeAndPassReceiver());
}
