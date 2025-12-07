// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CpuUsage} from '../model/cpu_usage_helper.js';

/**
 * Calculate the average CPU usage.
 *
 * @param usages - The usage for each CPUs.
 * @returns The average CPU usage.
 */
export function getAverageCpuUsage(usages: CpuUsage[]): CpuUsage {
  const output:
      CpuUsage = {userPercentage: 0, systemPercentage: 0, idlePercentage: 0};

  const count = usages.length;
  if (count === 0) {
    return output;
  }

  for (const usage of usages) {
    output.userPercentage += usage.userPercentage;
    output.systemPercentage += usage.systemPercentage;
    output.idlePercentage += usage.idlePercentage;
  }
  output.userPercentage /= count;
  output.systemPercentage /= count;
  output.idlePercentage /= count;
  return output;
}
