#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
 Convert the ASCII share_targets.asciipb proto into a binary resource.
"""

from __future__ import absolute_import
from __future__ import print_function
import os
import sys

# Import the binary proto generator. Walks up to the root of the source tree
# which is six directories above, and the finds the protobufs directory from
# there.
proto_generator_path = os.path.normpath(
    os.path.join(os.path.abspath(__file__),
                 *[os.path.pardir] * 6 + ['components/resources/protobufs']))
sys.path.insert(0, proto_generator_path)
from binary_proto_generator import BinaryProtoGenerator


def ParseInputPb(input_pb):
    """ Return a protobuf based on input pb """

    new_pb = share_target_pb2.MapLocaleTargets()

    temp_pb = share_target_pb2.TargetLocalesForParsing()
    temp_pb.CopyFrom(input_pb)

    new_pb.version_id = temp_pb.version_id

    all_targets_pb = share_target_pb2.TmpShareTargetMap()

    for s in temp_pb.targets:
        all_targets_pb.all_targets[s.nickname].nickname = s.nickname
        all_targets_pb.all_targets[s.nickname].url = s.url
        all_targets_pb.all_targets[s.nickname].icon = s.icon
        all_targets_pb.all_targets[s.nickname].icon_2x = s.icon_2x
        all_targets_pb.all_targets[s.nickname].icon_3x = s.icon_3x

    for s in temp_pb.locale_mapping:
        tmp_share_targets = share_target_pb2.ShareTargets()
        for target in s.targets:
            added = tmp_share_targets.targets.add()
            added.nickname = all_targets_pb.all_targets[target].nickname
            added.url = all_targets_pb.all_targets[target].url
            added.icon = all_targets_pb.all_targets[target].icon
            added.icon_2x = all_targets_pb.all_targets[target].icon_2x
            added.icon_3x = all_targets_pb.all_targets[target].icon_3x

        for locale in s.locale_keys:
            for target in tmp_share_targets.targets:
                added = new_pb.map_target_locale_map[locale].targets.add()
                added.nickname = target.nickname
                added.url = target.url
                added.icon = target.icon
                added.icon_2x = target.icon_2x
                added.icon_3x = target.icon_3x

    return new_pb


def ParsePbAndWrite(input_pb, outfile):
    parsed_pb = ParseInputPb(input_pb)

    binary_pb_str = parsed_pb.SerializeToString()

    open(outfile, 'wb').write(binary_pb_str)


class ShareTargetProtoGenerator(BinaryProtoGenerator):
    def ImportProtoModule(self):
        import share_target_pb2
        globals()['share_target_pb2'] = share_target_pb2

    def EmptyProtoInstance(self):
        return share_target_pb2.TargetLocalesForParsing()

    def ValidatePb(self, opts, pb):
        """ Validate the basic values of the protobuf."""
        assert pb.version_id > 0
        assert len(pb.locale_mapping) > 1
        assert len(pb.targets) > 1

    def ProcessPb(self, opts, pb):
        """ Generate one or more binary protos using the parsed proto. """
        outfile = os.path.join(opts.outdir, opts.outbasename)
        ParsePbAndWrite(pb, outfile)

    def VerifyArgs(self, opts):
        return True


def main():
    return ShareTargetProtoGenerator().Run()


if __name__ == '__main__':
    sys.exit(main())
