// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface GlicGuestLoadTimeData {
  loggingEnabled?: boolean;
  maxInFlightRequests?: number;
  sendResponsesForAllRequests?: boolean;
  chromeVersion?: string;
  chromeChannel?: string;
  glicHeaderRequestTypes?: string;
  enableStructuredYieldMetadata?: boolean;
}

export function getGuestLoadTimeData(): GlicGuestLoadTimeData {
  return (window as unknown as {
           glicGuestLoadTimeData?: GlicGuestLoadTimeData,
         }).glicGuestLoadTimeData ??
      {};
}
