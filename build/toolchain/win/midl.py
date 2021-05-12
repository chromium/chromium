# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import division
from __future__ import print_function

import array
import difflib
import distutils.dir_util
import filecmp
import io
import operator
import os
import posixpath
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import uuid

from functools import reduce


def ZapTimestamp(filename):
  contents = open(filename, 'rb').read()
  # midl.exe writes timestamp 2147483647 (2^31 - 1) as creation date into its
  # outputs, but using the local timezone.  To make the output timezone-
  # independent, replace that date with a fixed string of the same length.
  # Also blank out the minor version number.
  if filename.endswith('.tlb'):
    # See https://chromium-review.googlesource.com/c/chromium/src/+/693223 for
    # a fairly complete description of the .tlb binary format.
    # TLB files start with a 54 byte header. Offset 0x20 stores how many types
    # are defined in the file, and the header is followed by that many uint32s.
    # After that, 15 section headers appear.  Each section header is 16 bytes,
    # starting with offset and length uint32s.
    # Section 12 in the file contains custom() data. custom() data has a type
    # (int, string, etc).  Each custom data chunk starts with a uint16_t
    # describing its type.  Type 8 is string data, consisting of a uint32_t
    # len, followed by that many data bytes, followed by 'W' bytes to pad to a
    # 4 byte boundary.  Type 0x13 is uint32 data, followed by 4 data bytes,
    # followed by two 'W' to pad to a 4 byte boundary.
    # The custom block always starts with one string containing "Created by
    # MIDL version 8...", followed by one uint32 containing 0x7fffffff,
    # followed by another uint32 containing the MIDL compiler version (e.g.
    # 0x0801026e for v8.1.622 -- 0x26e == 622).  These 3 fields take 0x54 bytes.
    # There might be more custom data after that, but these 3 blocks are always
    # there for file-level metadata.
    # All data is little-endian in the file.
    assert contents[0:8] == b'MSFT\x02\x00\x01\x00'
    ntypes, = struct.unpack_from('<I', contents, 0x20)
    custom_off, custom_len = struct.unpack_from(
        '<II', contents, 0x54 + 4*ntypes + 11*16)
    assert custom_len >= 0x54
    # First: Type string (0x8), followed by 0x3e characters.
    assert contents[custom_off:custom_off + 6] == b'\x08\x00\x3e\x00\x00\x00'
    assert re.match(
        br'Created by MIDL version 8\.\d\d\.\d{4} '
        br'at ... Jan 1. ..:..:.. 2038\n',
        contents[custom_off + 6:custom_off + 6 + 0x3e])
    # Second: Type uint32 (0x13) storing 0x7fffffff (followed by WW / 0x57 pad)
    assert contents[custom_off+6+0x3e:custom_off+6+0x3e+8] == \
        b'\x13\x00\xff\xff\xff\x7f\x57\x57'
    # Third: Type uint32 (0x13) storing MIDL compiler version.
    assert contents[custom_off + 6 + 0x3e + 8:custom_off + 6 + 0x3e + 8 +
                    2] == b'\x13\x00'
    # Replace "Created by" string with fixed string, and fixed MIDL version with
    # 8.1.622 always.
    contents = (
        contents[0:custom_off + 6] +
        b'Created by MIDL version 8.xx.xxxx at a redacted point in time\n' +
        # uint32 (0x13) val 0x7fffffff, WW, uint32 (0x13), val 0x0801026e, WW
        b'\x13\x00\xff\xff\xff\x7f\x57\x57\x13\x00\x6e\x02\x01\x08\x57\x57' +
        contents[custom_off + 0x54:])
  else:
    contents = re.sub(
        br'File created by MIDL compiler version 8\.\d\d\.\d{4} \*/\r\n'
        br'/\* at ... Jan 1. ..:..:.. 2038',
        br'File created by MIDL compiler version 8.xx.xxxx */\r\n'
        br'/* at a redacted point in time', contents)
    contents = re.sub(
        br'    Oicf, W1, Zp8, env=(.....) \(32b run\), '
        br'target_arch=(AMD64|X86) 8\.\d\d\.\d{4}',
        br'    Oicf, W1, Zp8, env=\1 (32b run), target_arch=\2 8.xx.xxxx',
        contents)
    # TODO(thakis): If we need more hacks than these, try to verify checked-in
    # outputs when we're using the hermetic toolchain.
    # midl.exe older than 8.1.622 omit '//' after #endif, fix that:
    contents = contents.replace(b'#endif !_MIDL_USE_GUIDDEF_',
                                b'#endif // !_MIDL_USE_GUIDDEF_')
    # midl.exe puts the midl version into code in one place.  To have
    # predictable output, lie about the midl version if it's not 8.1.622.
    # This is unfortunate, but remember that there's beauty too in imperfection.
    contents = contents.replace(b'0x801026c, /* MIDL Version 8.1.620 */',
                                b'0x801026e, /* MIDL Version 8.1.622 */')
  open(filename, 'wb').write(contents)


def get_tlb_contents(tlb_file):
  # See ZapTimestamp() for a short overview of the .tlb format.
  contents = open(tlb_file, 'rb').read()
  assert contents[0:8] == b'MSFT\x02\x00\x01\x00'
  ntypes, = struct.unpack_from('<I', contents, 0x20)
  type_off, type_len = struct.unpack_from('<II', contents, 0x54 + 4*ntypes)

  guid_off, guid_len = struct.unpack_from(
      '<II', contents, 0x54 + 4*ntypes + 5*16)
  assert guid_len % 24 == 0

  contents = array.array('B', contents)

  return contents, ntypes, type_off, guid_off, guid_len


def recreate_guid_hashtable(contents, ntypes, guid_off, guid_len):
  # This function is called after changing guids in section 6 (the "guid"
  # section). This function recreates the GUID hashtable in section 5. Since the
  # hash table uses chaining, it's easiest to recompute it from scratch rather
  # than trying to patch it up.
  hashtab = [0xffffffff] * (0x80 // 4)
  for guidind in range(guid_off, guid_off + guid_len, 24):
    guidbytes, typeoff, nextguid = struct.unpack_from(
        '<16sII', contents, guidind)
    words = struct.unpack('<8H', guidbytes)
    # midl seems to use the following simple hash function for GUIDs:
    guidhash = reduce(operator.xor, [w for w in words]) % (0x80 // 4)
    nextguid = hashtab[guidhash]
    struct.pack_into('<I', contents, guidind + 0x14, nextguid)
    hashtab[guidhash] = guidind - guid_off
  hash_off, hash_len = struct.unpack_from(
      '<II', contents, 0x54 + 4*ntypes + 4*16)
  for i, hashval in enumerate(hashtab):
    struct.pack_into('<I', contents, hash_off + 4*i, hashval)


def overwrite_guids_h(h_file, dynamic_guids):
  contents = open(h_file, 'rb').read()
  for key in dynamic_guids:
    contents = re.sub(key, dynamic_guids[key], contents, flags=re.I)
  open(h_file, 'wb').write(contents)


def get_uuid_format(guid, prefix):
  formatted_uuid = b'0x%s,0x%s,0x%s,' % (guid[0:8], guid[9:13], guid[14:18])
  formatted_uuid += b'%s0x%s,0x%s' % (prefix, guid[19:21], guid[21:23])
  for i in range(24, len(guid), 2):
    formatted_uuid += b',0x' + guid[i:i + 2]
  return formatted_uuid


def get_uuid_format_iid_file(guid):
  # Convert from "D0E1CACC-C63C-4192-94AB-BF8EAD0E3B83" to
  # 0xD0E1CACC,0xC63C,0x4192,0x94,0xAB,0xBF,0x8E,0xAD,0x0E,0x3B,0x83.
  return get_uuid_format(guid, b'')


def overwrite_guids_iid(iid_file, dynamic_guids):
  contents = open(iid_file, 'rb').read()
  for key in dynamic_guids:
    contents = re.sub(get_uuid_format_iid_file(key),
                      get_uuid_format_iid_file(dynamic_guids[key]),
                      contents,
                      flags=re.I)
  open(iid_file, 'wb').write(contents)


def get_uuid_format_proxy_file(guid):
  # Convert from "D0E1CACC-C63C-4192-94AB-BF8EAD0E3B83" to
  # {0xD0E1CACC,0xC63C,0x4192,{0x94,0xAB,0xBF,0x8E,0xAD,0x0E,0x3B,0x83}}.
  return get_uuid_format(guid, b'{')


def overwrite_guids_proxy(proxy_file, dynamic_guids):
  contents = open(proxy_file, 'rb').read()
  for key in dynamic_guids:
    contents = re.sub(get_uuid_format_proxy_file(key),
                      get_uuid_format_proxy_file(dynamic_guids[key]),
                      contents,
                      flags=re.I)
  open(proxy_file, 'wb').write(contents)


def getguid(contents, offset):
  # Returns a guid string of the form "D0E1CACC-C63C-4192-94AB-BF8EAD0E3B83".
  g0, g1, g2, g3 = struct.unpack_from('<IHH8s', contents, offset)
  g3 = b''.join([b'%02X' % g for g in bytearray(g3)])
  return b'%08X-%04X-%04X-%s-%s' % (g0, g1, g2, g3[0:4], g3[4:])


def setguid(contents, offset, guid):
  guid = uuid.UUID(guid.decode('utf-8'))
  struct.pack_into('<IHH8s', contents, offset,
                   *(guid.fields[0:3] + (guid.bytes[8:], )))


def overwrite_guids_tlb(tlb_file, dynamic_guids):
  contents, ntypes, type_off, guid_off, guid_len = get_tlb_contents(tlb_file)

  for i in range(0, guid_len, 24):
    current_guid = getguid(contents, guid_off + i)
    for key in dynamic_guids:
      if key.lower() == current_guid.lower():
        setguid(contents, guid_off + i, dynamic_guids[key])

  recreate_guid_hashtable(contents, ntypes, guid_off, guid_len)
  open(tlb_file, 'wb').write(contents)


# Handle multiple guid substitutions, where |dynamic_guids| is of the form
# "PLACEHOLDER-GUID-158428a4-6014-4978-83ba-9fad0dabe791="
# "3d852661-c795-4d20-9b95-5561e9a1d2d9,"
# "PLACEHOLDER-GUID-63B8FFB1-5314-48C9-9C57-93EC8BC6184B="
# "D0E1CACC-C63C-4192-94AB-BF8EAD0E3B83".
#
# Before specifying |dynamic_guids| in the build, the IDL file is first compiled
# with "158428a4-6014-4978-83ba-9fad0dabe791" and
# "63B8FFB1-5314-48C9-9C57-93EC8BC6184B". These are the "replaceable" guids,
# i.e., guids that can be replaced in future builds. The resulting MIDL outputs
# are copied over to src\third_party\win_build_output\.
#
# Then, in the future, any changes to these guids can be accomplished by
# providing |dynamic_guids| of the format above in the build file. These
# "dynamic" guid changes by themselves will not require the MIDL compiler and
# therefore will not require copying output over to
# src\third_party\win_build_output\.
#
# The pre-generated src\third_party\win_build_output\ files are used for
# cross-compiling on other platforms, since the MIDL compiler is Windows-only.
def overwrite_guids(h_file, iid_file, proxy_file, tlb_file, dynamic_guids):
  # Fix up GUIDs in .h, _i.c, _p.c, and .tlb.
  overwrite_guids_h(h_file, dynamic_guids)
  overwrite_guids_iid(iid_file, dynamic_guids)
  overwrite_guids_proxy(proxy_file, dynamic_guids)
  if tlb_file:
    overwrite_guids_tlb(tlb_file, dynamic_guids)


# This function removes all occurrences of 'PLACEHOLDER-GUID-' from the
# template, and if |dynamic_guids| is specified, also replaces the guids within
# the file. Finally, it writes the resultant output to the |idl| file.
def generate_idl_from_template(idl_template, dynamic_guids, idl):
  contents = open(idl_template, 'rb').read()
  contents = re.sub(b'PLACEHOLDER-GUID-', b'', contents, flags=re.I)
  if dynamic_guids:
    for key in dynamic_guids:
      contents = re.sub(key, dynamic_guids[key], contents, flags=re.I)
  open(idl, 'wb').write(contents)


# This function runs the MIDL compiler with the provided arguments. It creates
# and returns a tuple of |0,midl_output_dir| on success.
def run_midl(args, env_dict):
  midl_output_dir = tempfile.mkdtemp()
  delete_midl_output_dir = True

  try:
    popen = subprocess.Popen(args + ['/out', midl_output_dir],
                             shell=True,
                             universal_newlines=True,
                             env=env_dict,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    out, _ = popen.communicate()
    if popen.returncode != 0:
      return popen.returncode, midl_output_dir

    # Filter junk out of stdout, and write filtered versions. Output we want
    # to filter is pairs of lines that look like this:
    # Processing C:\Program Files (x86)\Microsoft SDKs\...\include\objidl.idl
    # objidl.idl
    lines = out.splitlines()
    prefixes = ('Processing ', '64 bit Processing ')
    processing = set(
        os.path.basename(x) for x in lines if x.startswith(prefixes))
    for line in lines:
      if not line.startswith(prefixes) and line not in processing:
        print(line)

    for f in os.listdir(midl_output_dir):
      ZapTimestamp(os.path.join(midl_output_dir, f))

    delete_midl_output_dir = False
  finally:
    if os.path.exists(midl_output_dir) and delete_midl_output_dir:
      shutil.rmtree(midl_output_dir)

  return 0, midl_output_dir


# This function adds support for dynamic generation of guids: when values are
# specified as 'uuid5:name', this function will substitute the values with
# generated dynamic guids using the uuid5 function. The uuid5 function generates
# a guid based on the SHA-1 hash of a namespace identifier (which is the guid
# that comes after 'PLACEHOLDER-GUID-') and a name (which is a string, such as a
# version string "87.1.2.3").
#
# For instance, when |dynamic_guid| is of the form:
# "PLACEHOLDER-GUID-158428a4-6014-4978-83ba-9fad0dabe791=uuid5:88.0.4307.0
# ,"
# "PLACEHOLDER-GUID-63B8FFB1-5314-48C9-9C57-93EC8BC6184B=uuid5:88.0.4307.0
# "
#
# "PLACEHOLDER-GUID-158428a4-6014-4978-83ba-9fad0dabe791" would be substituted
# with uuid5("158428a4-6014-4978-83ba-9fad0dabe791", "88.0.4307.0"), which is
# "64700170-AD80-5DE3-924E-2F39D862CFD5". And
# "PLACEHOLDER-GUID-63B8FFB1-5314-48C9-9C57-93EC8BC6184B" would be
# substituted with uuid5("63B8FFB1-5314-48C9-9C57-93EC8BC6184B", "88.0.4307.0"),
# which is "7B6E7538-3C38-5565-BC92-42BCEE268D76".
def uuid5_substitutions(dynamic_guids):
  for key, value in dynamic_guids.items():
    if value.startswith('uuid5:'):
      name = value.split('uuid5:', 1)[1]
      assert name
      dynamic_guids[key] = str(uuid.uuid5(uuid.UUID(key), name)).upper()


def main(arch, gendir, outdir, dynamic_guids, tlb, h, dlldata, iid, proxy,
         clang, idl, *flags):
  # Copy checked-in outputs to final location.
  source = gendir
  if os.path.isdir(os.path.join(source, os.path.basename(idl))):
    source = os.path.join(source, os.path.basename(idl))
  source = os.path.join(source, arch.split('.')[1])  # Append 'x86' or 'x64'.
  source = os.path.normpath(source)

  source_exists = True
  if not os.path.isdir(source):
    source_exists = False
    if sys.platform != 'win32':
      print('Directory %s needs to be populated from Windows first' % source)
      return 1

    # This is a brand new IDL file that does not have outputs under
    # third_party\win_build_output\midl. We create an empty directory for now.
    os.makedirs(source)

  common_files = [h, iid]
  if tlb != 'none':
    # Not all projects use tlb files.
    common_files += [tlb]
  else:
    tlb = None

  if dlldata != 'none':
    # Not all projects use dlldta files.
    common_files += [dlldata]
  else:
    dlldata = None

  # Not all projects use proxy files
  if proxy != 'none':
    # Not all projects use proxy files.
    common_files += [proxy]
  else:
    proxy = None

  for source_file in common_files:
    file_path = os.path.join(source, source_file)
    if not os.path.isfile(file_path):
      source_exists = False
      if sys.platform != 'win32':
        print('File %s needs to be generated from Windows first' % file_path)
        return 1

      # Either this is a brand new IDL file that does not have outputs under
      # third_party\win_build_output\midl or the file is (unexpectedly) missing.
      # We create an empty file for now. The rest of the machinery below will
      # then generate the correctly populated file using the MIDL compiler and
      # instruct the developer to copy that file under
      # third_party\win_build_output\midl.
      open(file_path, 'wb').close()
    shutil.copy(file_path, outdir)

  if dynamic_guids != 'none':
    assert '=' in dynamic_guids
    if dynamic_guids.startswith("ignore_proxy_stub,"):
      # TODO(ganesh): The custom proxy/stub file ("_p.c") is not generated
      # correctly for dynamic IIDs (but correctly if there are only dynamic
      # CLSIDs). The proxy/stub lookup functions generated by MIDL.exe within
      # "_p.c" rely on a sorted set of vtable lists, which we are not currently
      # regenerating. At the moment, no project in Chromium that uses dynamic
      # IIDs is relying on the custom proxy/stub file. So for now, if
      # |dynamic_guids| is prefixed with "ignore_proxy_stub,", we exclude the
      # custom proxy/stub file from the directory comparisons.
      common_files.remove(proxy)
      dynamic_guids = dynamic_guids.split("ignore_proxy_stub,", 1)[1]
    dynamic_guids = re.sub('PLACEHOLDER-GUID-', '', dynamic_guids, flags=re.I)
    dynamic_guids = dynamic_guids.split(',')
    dynamic_guids = dict(s.split('=') for s in dynamic_guids)
    uuid5_substitutions(dynamic_guids)
    dynamic_guids_bytes = {
        k.encode('utf-8'): v.encode('utf-8')
        for k, v in dynamic_guids.items()
    }
    if source_exists:
      overwrite_guids(*(os.path.join(outdir, file) if file else None
                        for file in [h, iid, proxy, tlb]),
                      dynamic_guids=dynamic_guids_bytes)
  else:
    dynamic_guids = None

  # On non-Windows, that's all we can do.
  if sys.platform != 'win32':
    return 0

  idl_template = None
  if dynamic_guids:
    idl_template = idl

    # posixpath is used here to keep the MIDL-generated files with a uniform
    # separator of '/' instead of mixed '/' and '\\'.
    idl = posixpath.join(
        outdir,
        os.path.splitext(os.path.basename(idl_template))[0] + '.idl')

    # |idl_template| can contain one or more occurrences of guids that are
    # substituted with |dynamic_guids|, and then MIDL is run on the substituted
    # IDL file.
    generate_idl_from_template(idl_template, dynamic_guids_bytes, idl)

  # On Windows, run midl.exe on the input and check that its outputs are
  # identical to the checked-in outputs (after replacing guids if
  # |dynamic_guids| is specified).

  # Read the environment block from the file. This is stored in the format used
  # by CreateProcess. Drop last 2 NULs, one for list terminator, one for
  # trailing vs. separator.
  env_pairs = open(arch).read()[:-2].split('\0')
  env_dict = dict([item.split('=', 1) for item in env_pairs])

  # Extract the /D options and send them to the preprocessor.
  preprocessor_options = '-E -nologo -Wno-nonportable-include-path'
  preprocessor_options += ''.join(
      [' ' + flag for flag in flags if flag.startswith('/D')])
  args = ['midl', '/nologo'] + list(flags) + (['/tlb', tlb] if tlb else []) + [
      '/h', h
  ] + (['/dlldata', dlldata] if dlldata else []) + ['/iid', iid] + (
      ['/proxy', proxy] if proxy else
      []) + ['/cpp_cmd', clang, '/cpp_opt', preprocessor_options, idl]

  returncode, midl_output_dir = run_midl(args, env_dict)
  if returncode != 0:
    return returncode

  # Now compare the output in midl_output_dir to the copied-over outputs.
  _, mismatch, errors = filecmp.cmpfiles(midl_output_dir, outdir, common_files)
  assert not errors

  if mismatch:
    print('midl.exe output different from files in %s, see %s' %
          (outdir, midl_output_dir))
    for f in mismatch:
      if f.endswith('.tlb'): continue
      fromfile = os.path.join(outdir, f)
      tofile = os.path.join(midl_output_dir, f)
      print(''.join(
          difflib.unified_diff(
              io.open(fromfile).readlines(),
              io.open(tofile).readlines(), fromfile, tofile)))

    if dynamic_guids:
      # |idl_template| can contain one or more occurrences of guids prefixed
      # with 'PLACEHOLDER-GUID-'. We first remove the extraneous
      # 'PLACEHOLDER-GUID-' prefix and then run MIDL on the substituted IDL
      # file.
      # No guid substitutions are done at this point, because we want to compile
      # with the placeholder guids and then instruct the user to copy the output
      # over to |source| which is typically src\third_party\win_build_output\.
      # In future runs, the placeholder guids in |source| are replaced with the
      # guids specified in |dynamic_guids|.
      generate_idl_from_template(idl_template, None, idl)
      returncode, midl_output_dir = run_midl(args, env_dict)
      if returncode != 0:
        return returncode

    print('To rebaseline:')
    print(r'  copy /y %s\* %s' % (midl_output_dir, source))
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
