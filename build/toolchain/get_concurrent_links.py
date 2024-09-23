#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script computs the number of concurrent links we want to run in the build
# as a function of machine spec. It's based on GetDefaultConcurrentLinks in GYP.

import argparse
import multiprocessing
import os
import re
import subprocess
import sys

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
import gn_helpers


def _GetTotalMemoryInBytes():
  if sys.platform in ('win32', 'cygwin'):
    import ctypes

    class MEMORYSTATUSEX(ctypes.Structure):
      _fields_ = [
          ("dwLength", ctypes.c_ulong),
          ("dwMemoryLoad", ctypes.c_ulong),
          ("ullTotalPhys", ctypes.c_ulonglong),
          ("ullAvailPhys", ctypes.c_ulonglong),
          ("ullTotalPageFile", ctypes.c_ulonglong),
          ("ullAvailPageFile", ctypes.c_ulonglong),
          ("ullTotalVirtual", ctypes.c_ulonglong),
          ("ullAvailVirtual", ctypes.c_ulonglong),
          ("sullAvailExtendedVirtual", ctypes.c_ulonglong),
      ]

    stat = MEMORYSTATUSEX(dwLength=ctypes.sizeof(MEMORYSTATUSEX))
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(stat))
    return stat.ullTotalPhys
  elif sys.platform.startswith('linux'):
    if os.path.exists("/proc/meminfo"):
      with open("/proc/meminfo") as meminfo:
        memtotal_re = re.compile(r'^MemTotal:\s*(\d*)\s*kB')
        for line in meminfo:
          match = memtotal_re.match(line)
          if not match:
            continue
          return float(match.group(1)) * 2**10
  elif sys.platform == 'darwin':
    try:
      return int(subprocess.check_output(['sysctl', '-n', 'hw.memsize']))
    except Exception:
      return 0
  # TODO(scottmg): Implement this for other platforms.
  return 0


def _GetDefaultConcurrentLinks(per_link_gb, reserve_gb, thin_lto_type,
                               secondary_per_link_gb, override_ram_in_gb):
  explanation = []
  explanation.append(
      'per_link_gb={} reserve_gb={} secondary_per_link_gb={}'.format(
          per_link_gb, reserve_gb, secondary_per_link_gb))
  if override_ram_in_gb:
    mem_total_gb = override_ram_in_gb
  else:
    mem_total_gb = float(_GetTotalMemoryInBytes()) / 2**30
  adjusted_mem_total_gb = max(0, mem_total_gb - reserve_gb)

  # Ensure that there is at least as many links allocated for the secondary as
  # there is for the primary. The secondary link usually uses fewer gbs.
  mem_cap = int(
      max(1, adjusted_mem_total_gb / (per_link_gb + secondary_per_link_gb)))

  try:
    cpu_count = multiprocessing.cpu_count()
  except:
    cpu_count = 1

  # A local LTO links saturate all cores, but only for some amount of the link.
  cpu_cap = cpu_count
  if thin_lto_type is not None:
    assert thin_lto_type == 'local'
    cpu_cap = min(cpu_count, 6)

  explanation.append(
      'cpu_count={} cpu_cap={} mem_total_gb={:.1f}GiB adjusted_mem_total_gb={:.1f}GiB'
      .format(cpu_count, cpu_cap, mem_total_gb, adjusted_mem_total_gb))

  num_links = min(mem_cap, cpu_cap)
  if num_links == cpu_cap:
    if cpu_cap == cpu_count:
      reason = 'cpu_count'
    else:
      reason = 'cpu_cap (thinlto)'
  else:
    reason = 'RAM'

  # static link see too many open files if we have many concurrent links.
  # ref: http://b/233068481
  if num_links > 30:
    num_links = 30
    reason = 'nofile'

  explanation.append('concurrent_links={}  (reason: {})'.format(
      num_links, reason))

  # Use remaining RAM for a secondary pool if needed.
  if secondary_per_link_gb:
    mem_remaining = adjusted_mem_total_gb - num_links * per_link_gb
    secondary_size = int(max(0, mem_remaining / secondary_per_link_gb))
    if secondary_size > cpu_count:
      secondary_size = cpu_count
      reason = 'cpu_count'
    else:
      reason = 'mem_remaining={:.1f}GiB'.format(mem_remaining)
    explanation.append('secondary_size={} (reason: {})'.format(
        secondary_size, reason))
  else:
    secondary_size = 0

  return num_links, secondary_size, explanation


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--mem_per_link_gb', type=int, default=8)
  parser.add_argument('--reserve_mem_gb', type=int, default=0)
  parser.add_argument('--secondary_mem_per_link', type=int, default=0)
  parser.add_argument('--override-ram-in-gb-for-testing', type=float, default=0)
  parser.add_argument('--thin-lto')
  options = parser.parse_args()

  primary_pool_size, secondary_pool_size, explanation = (
      _GetDefaultConcurrentLinks(options.mem_per_link_gb,
                                 options.reserve_mem_gb, options.thin_lto,
                                 options.secondary_mem_per_link,
                                 options.override_ram_in_gb_for_testing))
  if options.override_ram_in_gb_for_testing:
    print('primary={} secondary={} explanation={}'.format(
        primary_pool_size, secondary_pool_size, explanation))
  else:
    sys.stdout.write(
        gn_helpers.ToGNString({
            'primary_pool_size': primary_pool_size,
            'secondary_pool_size': secondary_pool_size,
            'explanation': explanation,
        }))
  return 0


if __name__ == '__main__':
  sys.exit(main())
