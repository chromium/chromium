// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/url_session_test_util.h"

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chrome/browser/enterprise/platform_auth/url_session_url_loader.h"
#include "net/base/apple/url_conversions.h"

namespace url_session_test_util {

ResponseConfig::ResponseConfig()
    : body(std::nullopt), os_error(false), hang(false) {}
ResponseConfig::~ResponseConfig() = default;
ResponseConfig::ResponseConfig(ResponseConfig&&) = default;
ResponseConfig& ResponseConfig::operator=(ResponseConfig&&) = default;

}  // namespace url_session_test_util

using url_session_test_util::ResponseConfig;

@interface ResponseConfigHolder : NSObject {
  std::unique_ptr<ResponseConfig> _config;
}
- (instancetype)initWithConfig:(std::unique_ptr<ResponseConfig>)config;
- (ResponseConfig*)config;
@end

@implementation ResponseConfigHolder
- (instancetype)initWithConfig:(std::unique_ptr<ResponseConfig>)config {
  if (self = [super init]) {
    _config = std::move(config);
  }
  return self;
}

- (ResponseConfig*)config {
  return _config.get();
}
@end

static const char kConfigKey = 0;

@interface PlatformAuthStubNSURLProtocol : NSURLProtocol
@end

@implementation PlatformAuthStubNSURLProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest*)request {
  return YES;
}

+ (NSURLRequest*)canonicalRequestForRequest:(NSURLRequest*)request {
  return request;
}

- (void)startLoading {
  ResponseConfigHolder* holder =
      objc_getAssociatedObject([self class], &kConfigKey);

  ResponseConfig* config = [holder config];
  CHECK(config)
      << "Config not found. The protocol class was not initialized correctly.";

  if (config->on_started) {
    std::move(config->on_started).Run();
  }

  if (config->hang) {
    return;
  }

  if (config->os_error) {
    NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                         code:NSURLErrorNotConnectedToInternet
                                     userInfo:nil];
    [self.client URLProtocol:self didFailWithError:error];
    return;
  }

  NSData* data = nil;
  if (config->body.has_value()) {
    data = [NSData dataWithBytes:config->body.value().c_str()
                          length:config->body.value().size()];
  }

  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:self.request.URL
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:nil];

  [self.client URLProtocol:self
        didReceiveResponse:response
        cacheStoragePolicy:NSURLCacheStorageNotAllowed];
  [self.client URLProtocol:self didLoadData:data];
  [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading {
  ResponseConfigHolder* holder =
      objc_getAssociatedObject([self class], &kConfigKey);
  ResponseConfig* config = [holder config];

  if (config && config->on_stopped) {
    std::move(config->on_stopped).Run();
  }
}

@end

namespace url_session_test_util {

// Each created session takes ownership of the ResponseConfig by leveraging
// class association mechanism.
// ResponseConfig is wrapped with an Objective-C holder, which then is
// associated with the protocol class.
NSURLSession* GetTestURLSessionForConfig(ResponseConfig&& config) {
  Class base_class = [PlatformAuthStubNSURLProtocol class];

  // The class name has to be unique.
  static int id_counter = 0;
  const std::string class_name =
      "PlatformAuthStubNSURLProtocol_" + base::NumberToString(id_counter++);
  Class unique_subclass =
      objc_allocateClassPair(base_class, class_name.c_str(), 0);
  objc_registerClassPair(unique_subclass);

  auto config_ptr = std::make_unique<ResponseConfig>(std::move(config));
  ResponseConfigHolder* holder =
      [[ResponseConfigHolder alloc] initWithConfig:std::move(config_ptr)];
  // The config key is only used to differentiate associations in scope of a
  // single base class.
  objc_setAssociatedObject(unique_subclass, &kConfigKey, holder,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);

  NSURLSessionConfiguration* session_config =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  session_config.protocolClasses = @[ unique_subclass ];
  return [NSURLSession sessionWithConfiguration:session_config];
}

bool ScopedURLSessionOverrideForTesting::instance_exists_{false};

ScopedURLSessionOverrideForTesting::ScopedURLSessionOverrideForTesting(
    NSURLSession* session_override) {
  CHECK_IS_TEST();
  CHECK(!instance_exists_);
  enterprise_auth::URLSessionURLLoader::OverrideURLSessionForTesting(
      session_override);
  instance_exists_ = true;
}

ScopedURLSessionOverrideForTesting::~ScopedURLSessionOverrideForTesting() {
  enterprise_auth::URLSessionURLLoader::OverrideURLSessionForTesting(nil);
  instance_exists_ = false;
}

}  // namespace url_session_test_util
