// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// This is a helper application for launch_application_unittest.mm. This
// application records several events by writing them to a named pipe;
// the unit tests then use this information to verify that this helper was
// launched in the correct manner.
// The named pipe this writes to is equal to the name of the app bundle,
// with .app replaced by .fifo.

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate {
  NSArray* _command_line;
  NSURL* _fifo_url;
  NSRunningApplication* _running_app;
}

- (instancetype)initWithCommandLine:(NSArray*)command_line {
  self = [super init];
  if (self) {
    _command_line = command_line;
    NSURL* bundle_url = NSBundle.mainBundle.bundleURL;
    _fifo_url = [bundle_url.URLByDeletingLastPathComponent
        URLByAppendingPathComponent:
            [bundle_url.lastPathComponent
                stringByReplacingOccurrencesOfString:@".app"
                                          withString:@".fifo"]];
    _running_app = NSRunningApplication.currentApplication;
    [_running_app addObserver:self
                   forKeyPath:@"activationPolicy"
                      options:NSKeyValueObservingOptionNew
                      context:nil];
  }
  return self;
}

- (void)dealloc {
  [_running_app removeObserver:self forKeyPath:@"activationPolicy" context:nil];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  [self
      addLaunchEvent:@"activationPolicyChanged"
            withData:@{
              @"activationPolicy" : change[@"new"],
              @"processIdentifier" :
                  @(NSRunningApplication.currentApplication.processIdentifier),
            }];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  [self
      addLaunchEvent:@"applicationDidFinishLaunching"
            withData:@{
              @"activationPolicy" : @(NSApp.activationPolicy),
              @"commandLine" : _command_line,
              @"processIdentifier" :
                  @(NSRunningApplication.currentApplication.processIdentifier),
            }];
}

- (void)application:(NSApplication*)app openURLs:(NSArray<NSURL*>*)urls {
  [app replyToOpenOrPrint:NSApplicationDelegateReplySuccess];

  NSMutableArray* url_specs =
      [[NSMutableArray alloc] initWithCapacity:urls.count];
  for (NSURL* url in urls) {
    [url_specs addObject:url.absoluteString];
  }
  [self
      addLaunchEvent:@"openURLs"
            withData:@{
              @"activationPolicy" : @(NSApp.activationPolicy),
              @"processIdentifier" :
                  @(NSRunningApplication.currentApplication.processIdentifier),
              @"urls" : url_specs,
            }];
}

- (void)addLaunchEvent:(NSString*)event {
  [self addLaunchEvent:event withData:nil];
}

- (void)addLaunchEvent:(NSString*)event withData:(NSDictionary*)data {
  NSLog(@"Logging %@ with data %@", event, data);
  NSDictionary* event_dict = @{
    @"name" : event,
    @"data" : data,
  };
  // It is important to write this dictionary to the named pipe non-atomically,
  // as otherwise the write would replace the named pipe with a regular file
  // rather than writing to the pipe.
  NSData* plist_data = [NSPropertyListSerialization
      dataWithPropertyList:event_dict
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];
  [plist_data writeToURL:_fifo_url options:0 error:nil];
}

@end

__attribute__((visibility("default"))) int main(int argc, char** argv) {
  [NSApplication sharedApplication];

  NSMutableArray* command_line = [[NSMutableArray alloc] initWithCapacity:argc];
  for (int i = 0; i < argc; ++i) {
    [command_line addObject:[NSString stringWithUTF8String:argv[i]]];
  }

  AppDelegate* delegate =
      [[AppDelegate alloc] initWithCommandLine:command_line];
  NSApp.delegate = delegate;

  [NSApp run];
  return 0;
}
