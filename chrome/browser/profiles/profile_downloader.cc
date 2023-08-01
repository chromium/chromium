// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_downloader.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "skia/ext/image_operations.h"
#include "url/gurl.h"

namespace {

// Template for optional authorization header when using an OAuth access token.
constexpr char kAuthorizationHeader[] = "Bearer %s";

}  // namespace

ProfileDownloader::ProfileDownloader(ProfileDownloaderDelegate* delegate)
    : delegate_(delegate), identity_manager_(delegate_->GetIdentityManager()) {
  DCHECK(delegate_);
  identity_manager_observation_.Observe(identity_manager_.get());
}

void ProfileDownloader::Start() {
  StartForAccount(CoreAccountId());
}

void ProfileDownloader::StartForAccount(const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Starting profile downloader...";

  if (!identity_manager_) {
    // This can happen in some test paths.
    LOG(WARNING) << "User has no identity manager";
    delegate_->OnProfileDownloadFailure(
        this, ProfileDownloaderDelegate::TOKEN_ERROR);
    return;
  }

  account_id_ = account_id.empty() ? identity_manager_->GetPrimaryAccountId(
                                         signin::ConsentLevel::kSignin)
                                   : account_id;
  StartFetchingOAuth2AccessToken();
}

std::u16string ProfileDownloader::GetProfileFullName() const {
  return base::UTF8ToUTF16(account_info_.full_name);
}

std::u16string ProfileDownloader::GetProfileGivenName() const {
  return base::UTF8ToUTF16(account_info_.given_name);
}

std::string ProfileDownloader::GetProfileLocale() const {
  return account_info_.locale;
}

SkBitmap ProfileDownloader::GetProfilePicture() const {
  return profile_picture_;
}

ProfileDownloader::PictureStatus ProfileDownloader::GetProfilePictureStatus()
    const {
  return picture_status_;
}

std::string ProfileDownloader::GetProfilePictureURL() const {
  GURL url(account_info_.picture_url);
  if (!url.is_valid())
    return std::string();
  return signin::GetAvatarImageURLWithOptions(
             GURL(account_info_.picture_url),
             delegate_->GetDesiredImageSideLength(), true /* no_silhouette */)
      .spec();
}

void ProfileDownloader::StartFetchingImage() {
  VLOG(1) << "Fetching user entry with token: " << auth_token_;
  AccountInfo maybe_account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id_);
  if (!maybe_account_info.IsEmpty())
    account_info_ = maybe_account_info;

  if (account_info_.IsValid()) {
    // FetchImageData might call the delegate's OnProfileDownloadSuccess
    // synchronously, causing |this| to be deleted so there should not be more
    // code after it.
    FetchImageData();
  } else {
    waiting_for_account_info_ = true;
  }
}

void ProfileDownloader::StartFetchingOAuth2AccessToken() {
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
  // Required to determine if lock should be enabled.
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);

  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          account_id_, "profile_downloader", scopes,
          base::BindOnce(&ProfileDownloader::OnAccessTokenFetchComplete,
                         base::Unretained(this)),
          signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

ProfileDownloader::~ProfileDownloader() {
  oauth2_access_token_fetcher_.reset();
  DCHECK(
      identity_manager_observation_.IsObservingSource(identity_manager_.get()));
}

void ProfileDownloader::FetchImageData() {
  DCHECK(account_info_.IsValid());

  if (!delegate_->NeedsProfilePicture()) {
    VLOG(1) << "Skipping profile picture download";
    delegate_->OnProfileDownloadSuccess(this);
    return;
  }

  if (account_info_.picture_url == kNoPictureURLFound) {
    VLOG(1) << "No picture URL for account " << account_info_.email
            << ". Using the default profile picture.";
    picture_status_ = PICTURE_DEFAULT;
    delegate_->OnProfileDownloadSuccess(this);
    return;
  }

  std::string image_url_with_size = GetProfilePictureURL();
  if (!image_url_with_size.empty() &&
      image_url_with_size == delegate_->GetCachedPictureURL()) {
    VLOG(1) << "Picture URL matches cached picture URL";
    picture_status_ = PICTURE_CACHED;
    delegate_->OnProfileDownloadSuccess(this);
    return;
  }

  GURL image_url_to_fetch(image_url_with_size);
  if (!image_url_to_fetch.is_valid()) {
    VLOG(1) << "Profile picture URL with size |" << image_url_to_fetch << "| "
            << "is not valid (the account picture URL is "
            << "|" << account_info_.picture_url << "|)";
    delegate_->OnProfileDownloadFailure(
        this,
        ProfileDownloaderDelegate::FailureReason::INVALID_PROFILE_PICTURE_URL);
    return;
  }

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("signed_in_profile_avatar", R"(
        semantics {
          sender: "Profile Google Image Downloader"
          description:
            "Signed in users use their Google account image as their Chrome "
            "profile image, unless they explicitly select otherwise. This "
            "fetcher uses the sign-in token and the image URL provided by GAIA "
            "to fetch the image."
          trigger: "User signs into a Profile."
          data: "Filename of the png to download and Google OAuth bearer token."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/profiles/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: FILE_DATA
          }
          last_reviewed: "2023-07-31"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            UserAvatarCustomizationSelectorsEnabled {
              UserAvatarCustomizationSelectorsEnabled: false
            }
          }
        })");

  VLOG(1) << "Loading profile image from " << image_url_to_fetch;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = image_url_to_fetch;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (!auth_token_.empty()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StringPrintf(kAuthorizationHeader, auth_token_.c_str()));
  }

  network::mojom::URLLoaderFactory* loader_factory =
      delegate_->GetURLLoaderFactory();

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, base::BindOnce(&ProfileDownloader::OnURLLoaderComplete,
                                     base::Unretained(this)));
}

void ProfileDownloader::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int response_code = -1;
  if (simple_loader_->ResponseInfo() && simple_loader_->ResponseInfo()->headers)
    response_code = simple_loader_->ResponseInfo()->headers->response_code();

  if (response_body) {
    simple_loader_.reset();
    DVLOG(1) << "Decoding the image...";
    ImageDecoder::Start(this, std::move(*response_body));
  } else if (response_code == net::HTTP_NOT_FOUND) {
    simple_loader_.reset();
    VLOG(1) << "Got 404, using default picture...";
    picture_status_ = PICTURE_DEFAULT;
    delegate_->OnProfileDownloadSuccess(this);
  } else {
    LOG(WARNING) << "Loading profile data failed";
    DVLOG(1) << "  Error: " << simple_loader_->NetError();
    DVLOG(1) << "  Response code: " << response_code;
    DVLOG(1) << "  Url: " << simple_loader_->GetFinalURL().spec();
    // Handle miscellaneous 400/500 errors.
    bool network_error =
        response_code == -1 || (response_code >= 400 && response_code < 600);
    simple_loader_.reset();
    delegate_->OnProfileDownloadFailure(this, network_error ?
        ProfileDownloaderDelegate::NETWORK_ERROR :
        ProfileDownloaderDelegate::SERVICE_ERROR);
  }
}

void ProfileDownloader::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int image_size = delegate_->GetDesiredImageSideLength();
  profile_picture_ = skia::ImageOperations::Resize(
      decoded_image,
      skia::ImageOperations::RESIZE_BEST,
      image_size,
      image_size);
  picture_status_ = PICTURE_SUCCESS;
  delegate_->OnProfileDownloadSuccess(this);
}

void ProfileDownloader::OnDecodeImageFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnProfileDownloadFailure(
      this, ProfileDownloaderDelegate::IMAGE_DECODE_FAILED);
}

void ProfileDownloader::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth2_access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(WARNING)
        << "ProfileDownloader: token request using refresh token failed:"
        << error.ToString();
    delegate_->OnProfileDownloadFailure(this,
                                        ProfileDownloaderDelegate::TOKEN_ERROR);
    return;
  }
  auth_token_ = access_token_info.token;
  StartFetchingImage();
}

void ProfileDownloader::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  if (info.account_id == account_id_ && info.IsValid()) {
    account_info_ = info;

    // If the StartFetchingImage was called before we had valid info, the
    // downloader has been waiting so we need to fetch the image data now.
    if (waiting_for_account_info_) {
      waiting_for_account_info_ = false;
      // FetchImageData might call the delegate's OnProfileDownloadSuccess
      // synchronously, causing |this| to be deleted so there should not be more
      // code after it.
      FetchImageData();
    }
  }
}
