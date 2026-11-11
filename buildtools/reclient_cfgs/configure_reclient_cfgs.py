#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is used to fetch reclient cfgs."""

import argparse
import logging
import os
import posixpath
import re
import shutil
import subprocess
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC = os.path.abspath(os.path.join(THIS_DIR, "..", ".."))


def ClangRevision():
    sys.path.insert(0, os.path.join(CHROMIUM_SRC, "tools", "clang", "scripts"))
    import update

    sys.path.pop(0)
    return update.PACKAGE_VERSION


class CipdError(Exception):
    """Raised by configure_reclient_cfgs on fatal cipd error."""


class CipdAuthError(CipdError):
    """Raised by configure_reclient_cfgs on cipd auth error."""


def CipdEnsure(pkg_name, ref, directory, quiet):
    logging.info("ensure %s %s in %s", pkg_name, ref, directory)
    log_level = "warning" if quiet else "info"
    ensure_file = """
$ParanoidMode CheckIntegrity
{pkg} {ref}
""".format(pkg=pkg_name, ref=ref)
    try:
        output = subprocess.check_output(
            " ".join(
                [
                    "cipd",
                    "ensure",
                    "-log-level=" + log_level,
                    "-root",
                    directory,
                    "-ensure-file",
                    "-",
                ]
            ),
            shell=True,
            input=ensure_file,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
        logging.info(output)
    except subprocess.CalledProcessError as e:
        if not IsCipdLoggedIn():
            raise CipdAuthError(e.output) from e
        raise CipdError(e.output) from e


def IsCipdLoggedIn():
    ps = subprocess.run(
        ' '.join(['cipd', 'auth-info']),
        shell=True,
        capture_output=True,
        text=True,
    )
    logging.warning(
        "log for http://b/304677840: stdout from cipd auth-info: %s", ps.stdout
    )
    logging.warning(
        "log for http://b/304677840: stderr from cipd auth-info: %s", ps.stderr
    )
    return ps.returncode == 0


def RbeProjectFromInstance(instance):
    m = re.fullmatch(r"projects/([-\w]+)/instances/[-\w]+", instance)
    if not m:
        return None
    return m.group(1)


def RequestCipdAuthentication():
    """Requests that the user authenticate to access CIPD packages."""

    print("Access to remoteexec config CIPD package requires authentication.")
    print("-----------------------------------------------------------------")
    print()
    print("I'm sorry for the hassle, but you may need to do a one-time manual")
    print("authentication. Please run:")
    print()
    print("    update_depot_tools && cipd auth-login")
    print()
    print("and follow the instructions.")
    print()
    print("NOTE: Use your google.com credentials, not chromium.org.")
    print()
    print("-----------------------------------------------------------------")
    print()
    sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(
        description="configure reclient cfgs",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--rewrapper_cfg_project",
        "--rbe_project",
        help="RBE instance project id for rewrapper configs. "
        "Only specify if different from --rbe_instance\n"
        "TODO(b/270201776) --rbe_project is deprecated, "
        "remove once no more usages exist",
    )
    parser.add_argument(
        "--rbe_instance",
        help="RBE instance for rewrapper configs",
        default=os.environ.get("RBE_instance"),
    )
    parser.add_argument(
        "--cipd_prefix",
        help="cipd package name prefix",
        default="infra_internal/rbe/reclient_cfgs",
    )
    parser.add_argument(
        "--skip_remoteexec_cfg_fetch",
        help="skip downloading reclient cfgs from CIPD server",
        action="store_true",
    )
    parser.add_argument(
        "--quiet", help="Suppresses info logs", action="store_true"
    )

    args = parser.parse_args()

    logging.basicConfig(
        level=logging.WARNING if args.quiet else logging.INFO,
        format="%(message)s",
    )

    if not args.rewrapper_cfg_project and not args.rbe_instance:
        logging.error(
            "At least one of --rbe_instance or --rewrapper_cfg_project "
            "must be provided"
        )
        return 1

    rbe_project = args.rewrapper_cfg_project
    if not args.rewrapper_cfg_project:
        rbe_project = RbeProjectFromInstance(args.rbe_instance)

    # Remove obsolete configs from existing checkouts.
    for cfg in ["python", "reproxy.cfg", "win-cross"]:
        cfgpath = os.path.join(THIS_DIR, cfg)
        if os.path.isdir(cfgpath):

            def onerror(func, cfgpath, exc_info):
                os.chmod(cfgpath, 0o644)
                func(cfgpath)

            shutil.rmtree(cfgpath, onerror=onerror)
        elif os.path.exists(cfgpath):
            os.chmod(cfgpath, 0o644)
            os.remove(cfgpath)

    if args.skip_remoteexec_cfg_fetch:
        # remove stale reclient cfgs
        cfgpath = os.path.join(THIS_DIR, "chromium-browser-clang")
        if os.path.exists(cfgpath):

            def onerror(func, cfgpath, exc_info):
                os.chmod(cfgpath, 0o644)
                func(cfgpath)

            shutil.rmtree(cfgpath, onerror=onerror)
        return 0

    logging.info("fetch reclient_cfgs for RBE project %s..." % rbe_project)

    cipd_prefix = posixpath.join(args.cipd_prefix, rbe_project)

    tool_revisions = {
        "chromium-browser-clang": ClangRevision(),
    }
    for toolchain in tool_revisions:
        revision = tool_revisions[toolchain]
        if not revision:
            logging.info("failed to detect %s revision" % toolchain)
            continue

        toolchain_root = os.path.join(THIS_DIR, toolchain)
        cipd_ref = "revision/" + revision
        # 'cipd ensure' initializes the directory.
        try:
            CipdEnsure(
                posixpath.join(cipd_prefix, toolchain),
                ref=cipd_ref,
                directory=toolchain_root,
                quiet=args.quiet,
            )
        except CipdAuthError:
            RequestCipdAuthentication()
            return 1
        except CipdError as e:
            logging.error(e)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
