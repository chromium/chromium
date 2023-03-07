// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IOS_SIM_HEADER_SHIMS_H_
#define BASE_IOS_SIM_HEADER_SHIMS_H_

#include "build/blink_buildflags.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

#include <mach/kern_return.h>
#include <mach/message.h>
#include <sys/param.h>

// This file includes the necessary headers that are not part of the
// iOS public SDK in order to support multiprocess support on iOS.

__BEGIN_DECLS

#define BOOTSTRAP_MAX_NAME_LEN 128
typedef char name_t[BOOTSTRAP_MAX_NAME_LEN];
kern_return_t bootstrap_check_in(mach_port_t bp,
                                 const name_t service_name,
                                 mach_port_t* sp);
kern_return_t bootstrap_look_up(mach_port_t bp,
                                const name_t service_name,
                                mach_port_t* sp);
pid_t audit_token_to_pid(audit_token_t atoken);

const char* bootstrap_strerror(kern_return_t r);
#define BOOTSTRAP_SUCCESS 0
#define BOOTSTRAP_NOT_PRIVILEGED 1100
#define BOOTSTRAP_NAME_IN_USE 1101
#define BOOTSTRAP_UNKNOWN_SERVICE 1102
#define BOOTSTRAP_SERVICE_ACTIVE 1103
#define BOOTSTRAP_BAD_COUNT 1104
#define BOOTSTRAP_NO_MEMORY 1105
#define BOOTSTRAP_NO_CHILDREN 1106

int proc_pidpath(int pid, void* buffer, uint32_t buffersize);
#define PROC_PIDPATHINFO_MAXSIZE (4 * MAXPATHLEN)

__END_DECLS

#endif  // BASE_IOS_SIM_HEADER_SHIMS_H_
