#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install everything needed to build chromium (well, ideally, anyway)
# See https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md

usage() {
  echo "Usage: $0 [--options]"
  echo "Options:"
  echo "--[no-]syms: enable or disable installation of debugging symbols"
  echo "--lib32: enable installation of 32-bit libraries, e.g. for V8 snapshot"
  echo "--[no-]arm: enable or disable installation of arm cross toolchain"
  echo "--[no-]chromeos-fonts: enable or disable installation of Chrome OS"\
       "fonts"
  echo "--[no-]nacl: enable or disable installation of prerequisites for"\
       "building standalone NaCl and all its toolchains"
  echo "--[no-]backwards-compatible: enable or disable installation of packages
        that are no longer currently needed and have been removed from this
        script.  Useful for bisection."
  echo "--no-prompt: silently select standard options/defaults"
  echo "--quick-check: quickly try to determine if dependencies are installed"
  echo "               (this avoids interactive prompts and sudo commands,"
  echo "               so might not be 100% accurate)"
  echo "--unsupported: attempt installation even on unsupported systems"
  echo "Script will prompt interactively if options not given."
  exit 1
}

# Build list of apt packages in dpkg --get-selections format.
build_apt_package_list() {
  echo "Building apt package list." >&2
  apt-cache dumpavail | \
    python3 -c '\
      import re,sys; \
      o = sys.stdin.read(); \
      p = {"i386": ":i386"}; \
      f = re.M | re.S; \
      r = re.compile(r"^Package: (.+?)$.+?^Architecture: (.+?)$", f); \
      m = ["%s%s" % (x, p.get(y, "")) for x, y in re.findall(r, o)]; \
      print("\n".join(m))'
}

# Checks whether a particular package is available in the repos.
# Uses pre-formatted ${apt_package_list}.
# USAGE: $ package_exists <package name>
package_exists() {
  if [ -z "${apt_package_list}" ]; then
    echo "Call build_apt_package_list() prior to calling package_exists()" >&2
    apt_package_list=$(build_apt_package_list)
  fi
  # `grep` takes a regex string, so the +'s in package names, e.g. "libstdc++",
  # need to be escaped.
  local escaped="$(echo $1 | sed 's/[\~\+\.\:-]/\\&/g')"
  [ ! -z "$(grep "^${escaped}$" <<< "${apt_package_list}")" ]
}

# These default to on because (some) bots need them and it keeps things
# simple for the bot setup if all bots just run the script in its default
# mode.  Developers who don't want stuff they don't need installed on their
# own workstations can pass --no-arm --no-nacl when running the script.
do_inst_arm=1
do_inst_nacl=1

while [ "$1" != "" ]
do
  case "$1" in
  --syms)                    do_inst_syms=1;;
  --no-syms)                 do_inst_syms=0;;
  --lib32)                   do_inst_lib32=1;;
  --arm)                     do_inst_arm=1;;
  --no-arm)                  do_inst_arm=0;;
  --chromeos-fonts)          do_inst_chromeos_fonts=1;;
  --no-chromeos-fonts)       do_inst_chromeos_fonts=0;;
  --nacl)                    do_inst_nacl=1;;
  --no-nacl)                 do_inst_nacl=0;;
  --backwards-compatible)    do_inst_backwards_compatible=1;;
  --no-backwards-compatible) do_inst_backwards_compatible=0;;
  --add-cross-tool-repo)     add_cross_tool_repo=1;;
  --no-prompt)               do_default=1
                             do_quietly="-qq --assume-yes"
    ;;
  --quick-check)             do_quick_check=1;;
  --unsupported)             do_unsupported=1;;
  *) usage;;
  esac
  shift
done

if [ "$do_inst_arm" = "1" ]; then
  do_inst_lib32=1
fi

# Check for lsb_release command in $PATH
if ! which lsb_release > /dev/null; then
  echo "ERROR: lsb_release not found in \$PATH" >&2
  echo "try: sudo apt-get install lsb-release" >&2
  exit 1;
fi

distro_codename=$(lsb_release --codename --short)
distro_id=$(lsb_release --id --short)
# TODO(crbug.com/1199405): Remove 14.04 (trusty) and 16.04 (xenial).
supported_codenames="(trusty|xenial|bionic|disco|eoan|focal|groovy)"
supported_ids="(Debian)"
if [ 0 -eq "${do_unsupported-0}" ] && [ 0 -eq "${do_quick_check-0}" ] ; then
  if [[ ! $distro_codename =~ $supported_codenames &&
        ! $distro_id =~ $supported_ids ]]; then
    echo -e "ERROR: The only supported distros are\n" \
      "\tUbuntu 14.04 LTS (trusty with EoL April 2022)\n" \
      "\tUbuntu 16.04 LTS (xenial with EoL April 2024)\n" \
      "\tUbuntu 18.04 LTS (bionic with EoL April 2028)\n" \
      "\tUbuntu 20.04 LTS (focal with Eol April 2030)\n" \
      "\tUbuntu 20.10 (groovy)\n" \
      "\tDebian 10 (buster) or later" >&2
    exit 1
  fi

  if ! uname -m | egrep -q "i686|x86_64"; then
    echo "Only x86 architectures are currently supported" >&2
    exit
  fi
fi

if [ "x$(id -u)" != x0 ] && [ 0 -eq "${do_quick_check-0}" ]; then
  echo "Running as non-root user."
  echo "You might have to enter your password one or more times for 'sudo'."
  echo
fi

if [ 0 -eq "${do_quick_check-0}" ] ; then
  if [ "$do_inst_lib32" = "1" ] || [ "$do_inst_nacl" = "1" ]; then
    sudo dpkg --add-architecture i386
  fi
  sudo apt-get update
fi

# Populate ${apt_package_list} for package_exists() parsing.
apt_package_list=$(build_apt_package_list)

# Packages needed for chromeos only
chromeos_dev_list="libbluetooth-dev libxkbcommon-dev mesa-common-dev"

if package_exists realpath; then
  chromeos_dev_list="${chromeos_dev_list} realpath"
fi

# Packages needed for development
dev_list="\
  binutils
  bison
  bzip2
  cdbs
  curl
  dbus-x11
  dpkg-dev
  elfutils
  devscripts
  fakeroot
  flex
  git-core
  gperf
  libappindicator3-dev
  libasound2-dev
  libatspi2.0-dev
  libbrlapi-dev
  libbz2-dev
  libcairo2-dev
  libcap-dev
  libc6-dev
  libcups2-dev
  libcurl4-gnutls-dev
  libdrm-dev
  libelf-dev
  libevdev-dev
  libffi-dev
  libgbm-dev
  libglib2.0-dev
  libglu1-mesa-dev
  libgtk-3-dev
  libkrb5-dev
  libnspr4-dev
  libnss3-dev
  libpam0g-dev
  libpci-dev
  libpulse-dev
  libsctp-dev
  libspeechd-dev
  libsqlite3-dev
  libssl-dev
  libudev-dev
  libva-dev
  libwww-perl
  libxshmfence-dev
  libxslt1-dev
  libxss-dev
  libxt-dev
  libxtst-dev
  locales
  openbox
  p7zip
  patch
  perl
  pkg-config
  python-setuptools
  rpm
  ruby
  subversion
  uuid-dev
  wdiff
  x11-utils
  xcompmgr
  xz-utils
  zip
  $chromeos_dev_list
"

if package_exists python-is-python2; then
  dev_list="${dev_list} python-is-python2 python2-dev"
else
  dev_list="${dev_list} python python-dev"
fi

# 64-bit systems need a minimum set of 32-bit compat packages for the pre-built
# NaCl binaries.
if file -L /sbin/init | grep -q 'ELF 64-bit'; then
  dev_list="${dev_list} libc6-i386 lib32stdc++6"

  # lib32gcc-s1 used to be called lib32gcc1 in older distros.
  if package_exists lib32gcc-s1; then
    dev_list="${dev_list} lib32gcc-s1"
  elif package_exists lib32gcc1; then
    dev_list="${dev_list} lib32gcc1"
  fi
fi

# Run-time libraries required by chromeos only
chromeos_lib_list="libpulse0 libbz2-1.0"

# List of required run-time libraries
common_lib_list="\
  libappindicator3-1
  libasound2
  libatk1.0-0
  libatspi2.0-0
  libc6
  libcairo2
  libcap2
  libcups2
  libdrm2
  libevdev2
  libexpat1
  libfontconfig1
  libfreetype6
  libgbm1
  libglib2.0-0
  libgtk-3-0
  libpam0g
  libpango-1.0-0
  libpci3
  libpcre3
  libpixman-1-0
  libspeechd2
  libstdc++6
  libsqlite3-0
  libuuid1
  libwayland-egl1-mesa
  libx11-6
  libx11-xcb1
  libxau6
  libxcb1
  libxcomposite1
  libxcursor1
  libxdamage1
  libxdmcp6
  libxext6
  libxfixes3
  libxi6
  libxinerama1
  libxrandr2
  libxrender1
  libxtst6
  zlib1g
"

if package_exists libffi7; then
  common_lib_list="${common_lib_list} libffi7"
elif package_exists libffi6; then
  common_lib_list="${common_lib_list} libffi6"
fi

# Full list of required run-time libraries
lib_list="\
  $common_lib_list
  $chromeos_lib_list
"

# 32-bit libraries needed e.g. to compile V8 snapshot for Android or armhf
lib32_list="linux-libc-dev:i386 libpci3:i386"

# 32-bit libraries needed for a 32-bit build
lib32_list="$lib32_list libx11-xcb1:i386"

# Packages that have been removed from this script.  Regardless of configuration
# or options passed to this script, whenever a package is removed, it should be
# added here.
backwards_compatible_list="\
  7za
  fonts-indic
  fonts-ipafont
  fonts-stix
  fonts-thai-tlwg
  fonts-tlwg-garuda
  g++
  git-svn
  language-pack-da
  language-pack-fr
  language-pack-he
  language-pack-zh-hant
  libappindicator-dev
  libappindicator1
  libdconf-dev
  libdconf1
  libdconf1:i386
  libexif-dev
  libexif12
  libexif12:i386
  libgbm-dev
  libgconf-2-4:i386
  libgconf2-dev
  libgl1-mesa-dev
  libgl1-mesa-glx:i386
  libgles2-mesa-dev
  libgtk-3-0:i386
  libgtk2.0-0
  libgtk2.0-0:i386
  libgtk2.0-dev
  mesa-common-dev
  msttcorefonts
  ttf-dejavu-core
  ttf-indic-fonts
  ttf-kochi-gothic
  ttf-kochi-mincho
  ttf-mscorefonts-installer
  xfonts-mathml
"
if package_exists python-is-python2; then
  backwards_compatible_list="${backwards_compatible_list} python-dev"
fi

case $distro_codename in
  trusty)
    backwards_compatible_list+=" \
      libgbm-dev-lts-trusty
      libgl1-mesa-dev-lts-trusty
      libgl1-mesa-glx-lts-trusty:i386
      libgles2-mesa-dev-lts-trusty
      mesa-common-dev-lts-trusty"
    ;;
  xenial)
    backwards_compatible_list+=" \
      libgbm-dev-lts-xenial
      libgl1-mesa-dev-lts-xenial
      libgl1-mesa-glx-lts-xenial:i386
      libgles2-mesa-dev-lts-xenial
      mesa-common-dev-lts-xenial"
    ;;
esac

# arm cross toolchain packages needed to build chrome on armhf
EM_REPO="deb http://emdebian.org/tools/debian/ jessie main"
EM_SOURCE=$(cat <<EOF
# Repo added by Chromium $0
${EM_REPO}
# deb-src http://emdebian.org/tools/debian/ jessie main
EOF
)
EM_ARCHIVE_KEY_FINGER="084C6C6F39159EDB67969AA87DE089671804772E"
GPP_ARM_PACKAGE="g++-arm-linux-gnueabihf"
case $distro_codename in
  jessie)
    eval $(apt-config shell APT_SOURCESDIR 'Dir::Etc::sourceparts/d')
    CROSSTOOLS_LIST="${APT_SOURCESDIR}/crosstools.list"
    arm_list="libc6-dev:armhf
              linux-libc-dev:armhf"
    if [ "$do_inst_arm" = "1" ]; then
      if $(dpkg-query -W ${GPP_ARM_PACKAGE} &>/dev/null); then
        arm_list+=" ${GPP_ARM_PACKAGE}"
      else
        if [ "${add_cross_tool_repo}" = "1" ]; then
          gpg --keyserver pgp.mit.edu --recv-keys ${EM_ARCHIVE_KEY_FINGER}
          gpg -a --export ${EM_ARCHIVE_KEY_FINGER} | sudo apt-key add -
          if ! grep "^${EM_REPO}" "${CROSSTOOLS_LIST}" &>/dev/null; then
            echo "${EM_SOURCE}" | sudo tee -a "${CROSSTOOLS_LIST}" >/dev/null
          fi
          arm_list+=" ${GPP_ARM_PACKAGE}"
        else
          echo "The Debian Cross-toolchains repository is necessary to"
          echo "cross-compile Chromium for arm."
          echo "Rerun with --add-deb-cross-tool-repo to have it added for you."
        fi
      fi
    fi
    ;;
  # All necessary ARM packages are available on the default repos on
  # Debian 9 and later.
  *)
    arm_list="libc6-dev-armhf-cross
              linux-libc-dev-armhf-cross
              ${GPP_ARM_PACKAGE}"
    ;;
esac

# Work around for dependency issue Ubuntu/Trusty: http://crbug.com/435056
case $distro_codename in
  trusty)
    arm_list+=" g++-4.8-multilib-arm-linux-gnueabihf
                gcc-4.8-multilib-arm-linux-gnueabihf"
    ;;
  xenial|bionic)
    arm_list+=" g++-5-multilib-arm-linux-gnueabihf
                gcc-5-multilib-arm-linux-gnueabihf
                gcc-arm-linux-gnueabihf"
    ;;
  disco|eoan)
    arm_list+=" g++-9-multilib-arm-linux-gnueabihf
                gcc-9-multilib-arm-linux-gnueabihf
                gcc-arm-linux-gnueabihf"
    ;;
  focal)
    arm_list+=" g++-10-multilib-arm-linux-gnueabihf
                gcc-10-multilib-arm-linux-gnueabihf
                gcc-arm-linux-gnueabihf"
    ;;
  groovy)
    arm_list+=" g++-10-multilib-arm-linux-gnueabihf
                gcc-10-multilib-arm-linux-gnueabihf
                gcc-arm-linux-gnueabihf
                g++-10-arm-linux-gnueabihf
                gcc-10-arm-linux-gnueabihf"
    ;;
esac

# Packages to build NaCl, its toolchains, and its ports.
naclports_list="ant autoconf bison cmake gawk intltool xutils-dev xsltproc"
nacl_list="\
  g++-mingw-w64-i686
  lib32z1-dev
  libasound2:i386
  libcap2:i386
  libelf-dev:i386
  libfontconfig1:i386
  libglib2.0-0:i386
  libgpm2:i386
  libncurses5:i386
  lib32ncurses5-dev
  libnss3:i386
  libpango-1.0-0:i386
  libssl-dev:i386
  libtinfo-dev
  libtinfo-dev:i386
  libtool
  libuuid1:i386
  libxcomposite1:i386
  libxcursor1:i386
  libxdamage1:i386
  libxi6:i386
  libxrandr2:i386
  libxss1:i386
  libxtst6:i386
  texinfo
  xvfb
  ${naclports_list}
"

# Some package names have changed over time
if package_exists libssl1.1; then
  nacl_list="${nacl_list} libssl1.1:i386"
elif package_exists libssl1.0.2; then
  nacl_list="${nacl_list} libssl1.0.2:i386"
else
  nacl_list="${nacl_list} libssl1.0.0:i386"
fi
if package_exists libtinfo5; then
  nacl_list="${nacl_list} libtinfo5"
fi
if package_exists libpng16-16; then
  lib_list="${lib_list} libpng16-16"
else
  lib_list="${lib_list} libpng12-0"
fi
if package_exists libnspr4; then
  lib_list="${lib_list} libnspr4 libnss3"
else
  lib_list="${lib_list} libnspr4-0d libnss3-1d"
fi
if package_exists libjpeg-dev; then
  dev_list="${dev_list} libjpeg-dev"
else
  dev_list="${dev_list} libjpeg62-dev"
fi
if package_exists libudev1; then
  dev_list="${dev_list} libudev1"
  nacl_list="${nacl_list} libudev1:i386"
else
  dev_list="${dev_list} libudev0"
  nacl_list="${nacl_list} libudev0:i386"
fi
if package_exists libbrlapi0.8; then
  dev_list="${dev_list} libbrlapi0.8"
elif package_exists libbrlapi0.7; then
  dev_list="${dev_list} libbrlapi0.7"
elif package_exists libbrlapi0.6; then
  dev_list="${dev_list} libbrlapi0.6"
else
  dev_list="${dev_list} libbrlapi0.5"
fi
if package_exists apache2.2-bin; then
  dev_list="${dev_list} apache2.2-bin"
else
  dev_list="${dev_list} apache2-bin"
fi
if package_exists libav-tools; then
  dev_list="${dev_list} libav-tools"
fi
if package_exists php7.4-cgi; then
  dev_list="${dev_list} php7.4-cgi libapache2-mod-php7.4"
elif package_exists php7.3-cgi; then
  dev_list="${dev_list} php7.3-cgi libapache2-mod-php7.3"
elif package_exists php7.2-cgi; then
  dev_list="${dev_list} php7.2-cgi libapache2-mod-php7.2"
elif package_exists php7.1-cgi; then
  dev_list="${dev_list} php7.1-cgi libapache2-mod-php7.1"
elif package_exists php7.0-cgi; then
  dev_list="${dev_list} php7.0-cgi libapache2-mod-php7.0"
else
  dev_list="${dev_list} php5-cgi libapache2-mod-php5"
fi

# Most python 2 packages are removed in Ubuntu 20.10, but the build doesn't seem
# to need them, so only install them if they're available.
if package_exists python-crypto; then
  dev_list="${dev_list} python-crypto"
fi
if package_exists python-numpy; then
  dev_list="${dev_list} python-numpy"
fi
if package_exists python-openssl; then
  dev_list="${dev_list} python-openssl"
fi
if package_exists python-psutil; then
  dev_list="${dev_list} python-psutil"
fi
if package_exists python-yaml; then
  dev_list="${dev_list} python-yaml"
fi

# Some packages are only needed if the distribution actually supports
# installing them.
if package_exists appmenu-gtk; then
  lib_list="$lib_list appmenu-gtk"
fi
if package_exists libgnome-keyring0; then
  lib_list="${lib_list} libgnome-keyring0"
fi
if package_exists libgnome-keyring-dev; then
  lib_list="${lib_list} libgnome-keyring-dev"
fi
if package_exists libvulkan-dev; then
  dev_list="${dev_list} libvulkan-dev"
fi
if package_exists libvulkan1; then
  lib_list="${lib_list} libvulkan1"
fi
if package_exists libinput10; then
  lib_list="${lib_list} libinput10"
fi
if package_exists libinput-dev; then
    dev_list="${dev_list} libinput-dev"
fi
if package_exists snapcraft; then
    dev_list="${dev_list} snapcraft"
fi

# Cross-toolchain strip is needed for building the sysroots.
if package_exists binutils-arm-linux-gnueabihf; then
  dev_list="${dev_list} binutils-arm-linux-gnueabihf"
fi
if package_exists binutils-aarch64-linux-gnu; then
  dev_list="${dev_list} binutils-aarch64-linux-gnu"
fi
if package_exists binutils-mipsel-linux-gnu; then
  dev_list="${dev_list} binutils-mipsel-linux-gnu"
fi
if package_exists binutils-mips64el-linux-gnuabi64; then
  dev_list="${dev_list} binutils-mips64el-linux-gnuabi64"
fi

# When cross building for arm/Android on 64-bit systems the host binaries
# that are part of v8 need to be compiled with -m32 which means
# that basic multilib support is needed.
if file -L /sbin/init | grep -q 'ELF 64-bit'; then
  # gcc-multilib conflicts with the arm cross compiler (at least in trusty) but
  # g++-X.Y-multilib gives us the 32-bit support that we need. Find out the
  # appropriate value of X and Y by seeing what version the current
  # distribution's g++-multilib package depends on.
  multilib_package=$(apt-cache depends g++-multilib --important | \
      grep -E --color=never --only-matching '\bg\+\+-[0-9.]+-multilib\b')
  lib32_list="$lib32_list $multilib_package"
fi

if [ "$do_inst_syms" = "1" ]; then
  echo "Including debugging symbols."

  # Debian is in the process of transitioning to automatic debug packages, which
  # have the -dbgsym suffix (https://wiki.debian.org/AutomaticDebugPackages).
  # Untransitioned packages have the -dbg suffix.  And on some systems, neither
  # will be available, so exclude the ones that are missing.
  dbg_package_name() {
    if package_exists "$1-dbgsym"; then
      echo "$1-dbgsym"
    elif package_exists "$1-dbg"; then
      echo "$1-dbg"
    fi
  }

  for package in "${common_lib_list}"; do
    dbg_list="$dbg_list $(dbg_package_name ${package})"
  done

  # Debugging symbols packages not following common naming scheme
  if [ "$(dbg_package_name libstdc++6)" == "" ]; then
    if package_exists libstdc++6-8-dbg; then
      dbg_list="${dbg_list} libstdc++6-8-dbg"
    elif package_exists libstdc++6-7-dbg; then
      dbg_list="${dbg_list} libstdc++6-7-dbg"
    elif package_exists libstdc++6-6-dbg; then
      dbg_list="${dbg_list} libstdc++6-6-dbg"
    elif package_exists libstdc++6-5-dbg; then
      dbg_list="${dbg_list} libstdc++6-5-dbg"
    elif package_exists libstdc++6-4.9-dbg; then
      dbg_list="${dbg_list} libstdc++6-4.9-dbg"
    elif package_exists libstdc++6-4.8-dbg; then
      dbg_list="${dbg_list} libstdc++6-4.8-dbg"
    elif package_exists libstdc++6-4.7-dbg; then
      dbg_list="${dbg_list} libstdc++6-4.7-dbg"
    elif package_exists libstdc++6-4.6-dbg; then
      dbg_list="${dbg_list} libstdc++6-4.6-dbg"
    fi
  fi
  if [ "$(dbg_package_name libatk1.0-0)" == "" ]; then
    dbg_list="$dbg_list $(dbg_package_name libatk1.0)"
  fi
  if [ "$(dbg_package_name libpango-1.0-0)" == "" ]; then
    dbg_list="$dbg_list $(dbg_package_name libpango1.0-dev)"
  fi
else
  echo "Skipping debugging symbols."
  dbg_list=
fi

if [ "$do_inst_lib32" = "1" ]; then
  echo "Including 32-bit libraries."
else
  echo "Skipping 32-bit libraries."
  lib32_list=
fi

if [ "$do_inst_arm" = "1" ]; then
  echo "Including ARM cross toolchain."
else
  echo "Skipping ARM cross toolchain."
  arm_list=
fi

if [ "$do_inst_nacl" = "1" ]; then
  echo "Including NaCl, NaCl toolchain, NaCl ports dependencies."
else
  echo "Skipping NaCl, NaCl toolchain, NaCl ports dependencies."
  nacl_list=
fi

filtered_backwards_compatible_list=
if [ "$do_inst_backwards_compatible" = "1" ]; then
  echo "Including backwards compatible packages."
  for package in ${backwards_compatible_list}; do
    if package_exists ${package}; then
      filtered_backwards_compatible_list+=" ${package}"
    fi
  done
fi

# The `sort -r -s -t: -k2` sorts all the :i386 packages to the front, to avoid
# confusing dpkg-query (crbug.com/446172).
packages="$(
  echo "${dev_list} ${lib_list} ${dbg_list} ${lib32_list} ${arm_list}" \
       "${nacl_list}" ${filtered_backwards_compatible_list} | tr " " "\n" | \
       sort -u | sort -r -s -t: -k2 | tr "\n" " "
)"

if [ 1 -eq "${do_quick_check-0}" ] ; then
  if ! missing_packages="$(dpkg-query -W -f ' ' ${packages} 2>&1)"; then
    # Distinguish between packages that actually aren't available to the
    # system (i.e. not in any repo) and packages that just aren't known to
    # dpkg (i.e. managed by apt).
    missing_packages="$(echo "${missing_packages}" | awk '{print $NF}')"
    not_installed=""
    unknown=""
    for p in ${missing_packages}; do
      if apt-cache show ${p} > /dev/null 2>&1; then
        not_installed="${p}\n${not_installed}"
      else
        unknown="${p}\n${unknown}"
      fi
    done
    if [ -n "${not_installed}" ]; then
      echo "WARNING: The following packages are not installed:"
      echo -e "${not_installed}" | sed -e "s/^/  /"
    fi
    if [ -n "${unknown}" ]; then
      echo "WARNING: The following packages are unknown to your system"
      echo "(maybe missing a repo or need to 'sudo apt-get update'):"
      echo -e "${unknown}" | sed -e "s/^/  /"
    fi
    exit 1
  fi
  exit 0
fi

echo "Finding missing packages..."
# Intentionally leaving $packages unquoted so it's more readable.
echo "Packages required: " $packages
echo
query_cmd="apt-get --just-print install $(echo $packages)"
if cmd_output="$(LANGUAGE=en LANG=C $query_cmd)"; then
  new_list=$(echo "$cmd_output" |
    sed -e '1,/The following NEW packages will be installed:/d;s/^  //;t;d' |
    sed 's/ *$//')
  upgrade_list=$(echo "$cmd_output" |
    sed -e '1,/The following packages will be upgraded:/d;s/^  //;t;d' |
    sed 's/ *$//')
  if [ -z "$new_list" ] && [ -z "$upgrade_list" ]; then
    echo "No missing packages, and the packages are up to date."
  else
    echo "Installing and upgrading packages: $new_list $upgrade_list."
    sudo apt-get install ${do_quietly-} ${new_list} ${upgrade_list}
  fi
  echo
else
  # An apt-get exit status of 100 indicates that a real error has occurred.

  # I am intentionally leaving out the '"'s around query_cmd,
  # as this makes it easier to cut and paste the output
  echo "The following command failed: " ${query_cmd}
  echo
  echo "It produced the following output:"
  echo "$cmd_output"
  echo
  echo "You will have to install the above packages yourself."
  echo
  exit 100
fi

# Install the Chrome OS default fonts. This must go after running
# apt-get, since install-chromeos-fonts depends on curl.
if [ "$do_inst_chromeos_fonts" != "0" ]; then
  echo
  echo "Installing Chrome OS fonts."
  dir=`echo $0 | sed -r -e 's/\/[^/]+$//'`
  if ! sudo $dir/linux/install-chromeos-fonts.py; then
    echo "ERROR: The installation of the Chrome OS default fonts failed."
    if [ `stat -f -c %T $dir` == "nfs" ]; then
      echo "The reason is that your repo is installed on a remote file system."
    else
      echo "This is expected if your repo is installed on a remote file system."
    fi
    echo "It is recommended to install your repo on a local file system."
    echo "You can skip the installation of the Chrome OS default founts with"
    echo "the command line option: --no-chromeos-fonts."
    exit 1
  fi
else
  echo "Skipping installation of Chrome OS fonts."
fi

echo "Installing locales."
CHROMIUM_LOCALES="da_DK.UTF-8 fr_FR.UTF-8 he_IL.UTF-8 zh_TW.UTF-8"
LOCALE_GEN=/etc/locale.gen
if [ -e ${LOCALE_GEN} ]; then
  OLD_LOCALE_GEN="$(cat /etc/locale.gen)"
  for CHROMIUM_LOCALE in ${CHROMIUM_LOCALES}; do
    sudo sed -i "s/^# ${CHROMIUM_LOCALE}/${CHROMIUM_LOCALE}/" ${LOCALE_GEN}
  done
  # Regenerating locales can take a while, so only do it if we need to.
  if (echo "${OLD_LOCALE_GEN}" | cmp -s ${LOCALE_GEN}); then
    echo "Locales already up-to-date."
  else
    sudo locale-gen
  fi
else
  for CHROMIUM_LOCALE in ${CHROMIUM_LOCALES}; do
    sudo locale-gen ${CHROMIUM_LOCALE}
  done
fi
