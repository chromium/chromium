#!/bin/bash

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#@ This script builds Debian sysroot images for building Google Chrome.
#@
#@  Usage:
#@    sysroot-creator.sh {build,upload} \
#@    {amd64,i386,armhf,arm64,armel,mipsel,mips64el}
#@

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

DISTRO=debian
RELEASE=bullseye

# This number is appended to the sysroot key to cause full rebuilds.  It
# should be incremented when removing packages or patching existing packages.
# It should not be incremented when adding packages.
SYSROOT_RELEASE=2

ARCHIVE_TIMESTAMP=20230611T210420Z

ARCHIVE_URL="https://snapshot.debian.org/archive/debian/$ARCHIVE_TIMESTAMP/"
APT_SOURCES_LIST=(
  # Debian 12 (Bookworm) is needed for GTK4.  It should be kept before bullseye
  # so that bullseye takes precedence.
  "${ARCHIVE_URL} bookworm main"
  "${ARCHIVE_URL} bookworm-updates main"

  # This mimics a sources.list from bullseye.
  "${ARCHIVE_URL} bullseye main contrib non-free"
  "${ARCHIVE_URL} bullseye-updates main contrib non-free"
  "${ARCHIVE_URL} bullseye-backports main contrib non-free"
)

# gpg keyring file generated using generate_keyring.sh
KEYRING_FILE="${SCRIPT_DIR}/keyring.gpg"

# Sysroot packages: these are the packages needed to build chrome.
DEBIAN_PACKAGES="\
  comerr-dev
  krb5-multidev
  libasound2
  libasound2-dev
  libasyncns0
  libatk-bridge2.0-0
  libatk-bridge2.0-dev
  libatk1.0-0
  libatk1.0-dev
  libatomic1
  libatspi2.0-0
  libatspi2.0-dev
  libattr1
  libaudit1
  libavahi-client3
  libavahi-common3
  libb2-1
  libblkid-dev
  libblkid1
  libbluetooth-dev
  libbluetooth3
  libbrotli-dev
  libbrotli1
  libbsd0
  libc6
  libc6-dev
  libcairo-gobject2
  libcairo-script-interpreter2
  libcairo2
  libcairo2-dev
  libcap-dev
  libcap-ng0
  libcap2
  libcloudproviders0
  libcolord2
  libcom-err2
  libcrypt-dev
  libcrypt1
  libcups2
  libcups2-dev
  libcupsimage2
  libcupsimage2-dev
  libcurl3-gnutls
  libcurl4-gnutls-dev
  libdatrie-dev
  libdatrie1
  libdb5.3
  libdbus-1-3
  libdbus-1-dev
  libdbus-glib-1-2
  libdbusmenu-glib-dev
  libdbusmenu-glib4
  libdbusmenu-gtk3-4
  libdbusmenu-gtk4
  libdeflate-dev
  libdeflate0
  libdouble-conversion3
  libdrm-amdgpu1
  libdrm-dev
  libdrm-nouveau2
  libdrm-radeon1
  libdrm2
  libegl-dev
  libegl1
  libegl1-mesa
  libegl1-mesa-dev
  libelf-dev
  libelf1
  libepoxy-dev
  libepoxy0
  libevdev-dev
  libevdev2
  libevent-2.1-7
  libexpat1
  libexpat1-dev
  libffi-dev
  libffi7
  libflac-dev
  libflac8
  libfontconfig-dev
  libfontconfig1
  libfreetype-dev
  libfreetype6
  libfribidi-dev
  libfribidi0
  libgbm-dev
  libgbm1
  libgcc-10-dev
  libgcc-s1
  libgcrypt20
  libgcrypt20-dev
  libgdk-pixbuf-2.0-0
  libgdk-pixbuf-2.0-dev
  libgl-dev
  libgl1
  libgl1-mesa-dev
  libgl1-mesa-glx
  libglapi-mesa
  libgles-dev
  libgles1
  libgles2
  libglib2.0-0
  libglib2.0-dev
  libglvnd-dev
  libglvnd0
  libglx-dev
  libglx0
  libgmp10
  libgnutls-dane0
  libgnutls-openssl27
  libgnutls28-dev
  libgnutls30
  libgnutlsxx28
  libgomp1
  libgpg-error-dev
  libgpg-error0
  libgraphene-1.0-0
  libgraphene-1.0-dev
  libgraphite2-3
  libgraphite2-dev
  libgssapi-krb5-2
  libgssrpc4
  libgtk-3-0
  libgtk-3-dev
  libgtk-4-1
  libgtk-4-dev
  libgtk2.0-0
  libgudev-1.0-0
  libharfbuzz-dev
  libharfbuzz-gobject0
  libharfbuzz-icu0
  libharfbuzz0b
  libhogweed6
  libice6
  libicu-le-hb0
  libicu67
  libidl-2-0
  libidn11
  libidn2-0
  libinput-dev
  libinput10
  libjbig-dev
  libjbig0
  libjpeg62-turbo
  libjpeg62-turbo-dev
  libjson-glib-1.0-0
  libjsoncpp-dev
  libjsoncpp24
  libk5crypto3
  libkadm5clnt-mit12
  libkadm5srv-mit12
  libkdb5-10
  libkeyutils1
  libkrb5-3
  libkrb5-dev
  libkrb5support0
  liblcms2-2
  libldap-2.4-2
  liblerc4
  libltdl7
  liblz4-1
  liblzma5
  liblzo2-2
  libmd0
  libmd4c0
  libminizip-dev
  libminizip1
  libmount-dev
  libmount1
  libmtdev1
  libncurses-dev
  libncurses6
  libncursesw6
  libnettle8
  libnghttp2-14
  libnsl2
  libnspr4
  libnspr4-dev
  libnss-db
  libnss3
  libnss3-dev
  libogg-dev
  libogg0
  libopengl0
  libopus-dev
  libopus0
  libp11-kit0
  libpam0g
  libpam0g-dev
  libpango-1.0-0
  libpango1.0-dev
  libpangocairo-1.0-0
  libpangoft2-1.0-0
  libpangox-1.0-0
  libpangoxft-1.0-0
  libpci-dev
  libpci3
  libpciaccess0
  libpcre16-3
  libpcre2-16-0
  libpcre2-32-0
  libpcre2-8-0
  libpcre2-dev
  libpcre2-posix2
  libpcre3
  libpcre3-dev
  libpcre32-3
  libpcrecpp0v5
  libpipewire-0.3-0
  libpipewire-0.3-dev
  libpixman-1-0
  libpixman-1-dev
  libpng-dev
  libpng16-16
  libproxy1v5
  libpsl5
  libpthread-stubs0-dev
  libpulse-dev
  libpulse-mainloop-glib0
  libpulse0
  libqt5concurrent5
  libqt5core5a
  libqt5dbus5
  libqt5gui5
  libqt5network5
  libqt5printsupport5
  libqt5sql5
  libqt5test5
  libqt5widgets5
  libqt5xml5
  libqt6concurrent6
  libqt6core6
  libqt6dbus6
  libqt6gui6
  libqt6network6
  libqt6opengl6
  libqt6openglwidgets6
  libqt6printsupport6
  libqt6sql6
  libqt6test6
  libqt6widgets6
  libqt6xml6
  libre2-9
  libre2-dev
  librest-0.7-0
  librtmp1
  libsasl2-2
  libselinux1
  libselinux1-dev
  libsepol1
  libsepol1-dev
  libsm6
  libsnappy-dev
  libsnappy1v5
  libsndfile1
  libsoup-gnome2.4-1
  libsoup2.4-1
  libspa-0.2-dev
  libspeechd-dev
  libspeechd2
  libsqlite3-0
  libssh2-1
  libssl-dev
  libssl1.1
  libstdc++-10-dev
  libstdc++6
  libsystemd-dev
  libsystemd0
  libtasn1-6
  libthai-dev
  libthai0
  libtiff-dev
  libtiff5
  libtiff6
  libtiffxx5
  libtinfo6
  libtirpc3
  libts0
  libudev-dev
  libudev1
  libunbound8
  libunistring2
  libutempter-dev
  libutempter0
  libuuid1
  libva-dev
  libva-drm2
  libva-glx2
  libva-wayland2
  libva-x11-2
  libva2
  libvorbis0a
  libvorbisenc2
  libvulkan-dev
  libvulkan1
  libwacom2
  libwayland-bin
  libwayland-client0
  libwayland-cursor0
  libwayland-dev
  libwayland-egl-backend-dev
  libwayland-egl1
  libwayland-egl1-mesa
  libwayland-server0
  libwebp-dev
  libwebp6
  libwebp7
  libwebpdemux2
  libwebpmux3
  libwrap0
  libx11-6
  libx11-dev
  libx11-xcb-dev
  libx11-xcb1
  libxau-dev
  libxau6
  libxcb-dri2-0
  libxcb-dri2-0-dev
  libxcb-dri3-0
  libxcb-dri3-dev
  libxcb-glx0
  libxcb-glx0-dev
  libxcb-icccm4
  libxcb-image0
  libxcb-image0-dev
  libxcb-keysyms1
  libxcb-present-dev
  libxcb-present0
  libxcb-randr0
  libxcb-randr0-dev
  libxcb-render-util0
  libxcb-render-util0-dev
  libxcb-render0
  libxcb-render0-dev
  libxcb-shape0
  libxcb-shape0-dev
  libxcb-shm0
  libxcb-shm0-dev
  libxcb-sync-dev
  libxcb-sync1
  libxcb-util-dev
  libxcb-util1
  libxcb-xfixes0
  libxcb-xfixes0-dev
  libxcb-xinerama0
  libxcb-xinput0
  libxcb-xkb1
  libxcb1
  libxcb1-dev
  libxcomposite-dev
  libxcomposite1
  libxcursor-dev
  libxcursor1
  libxdamage-dev
  libxdamage1
  libxdmcp-dev
  libxdmcp6
  libxext-dev
  libxext6
  libxfixes-dev
  libxfixes3
  libxft-dev
  libxft2
  libxi-dev
  libxi6
  libxinerama-dev
  libxinerama1
  libxkbcommon-dev
  libxkbcommon-x11-0
  libxkbcommon0
  libxml2
  libxml2-dev
  libxrandr-dev
  libxrandr2
  libxrender-dev
  libxrender1
  libxshmfence-dev
  libxshmfence1
  libxslt1-dev
  libxslt1.1
  libxss-dev
  libxss1
  libxt-dev
  libxt6
  libxtst-dev
  libxtst6
  libxxf86vm-dev
  libxxf86vm1
  libzstd1
  linux-libc-dev
  mesa-common-dev
  qt6-base-dev
  qt6-base-dev-tools
  qtbase5-dev
  qtbase5-dev-tools
  shared-mime-info
  uuid-dev
  wayland-protocols
  x11proto-dev
  zlib1g
  zlib1g-dev
"

DEBIAN_PACKAGES_AMD64="
  libasan6
  libdrm-intel1
  libitm1
  liblsan0
  libquadmath0
  libtsan0
  libubsan1
  valgrind
"

DEBIAN_PACKAGES_I386="
  libasan6
  libdrm-intel1
  libitm1
  libquadmath0
  libubsan1
  valgrind
"

DEBIAN_PACKAGES_ARMHF="
  libasan6
  libdrm-etnaviv1
  libdrm-exynos1
  libdrm-freedreno1
  libdrm-omap1
  libdrm-tegra0
  libubsan1
  valgrind
"

DEBIAN_PACKAGES_ARM64="
  libasan6
  libdrm-etnaviv1
  libdrm-freedreno1
  libdrm-tegra0
  libgmp10
  libitm1
  liblsan0
  libthai0
  libtsan0
  libubsan1
  valgrind
"

DEBIAN_PACKAGES_ARMEL="
  libasan6
  libdrm-exynos1
  libdrm-freedreno1
  libdrm-omap1
  libdrm-tegra0
  libubsan1
"

DEBIAN_PACKAGES_MIPSEL="
"

DEBIAN_PACKAGES_MIPS64EL="
  valgrind
"

readonly REQUIRED_TOOLS="curl xzcat"

######################################################################
# Package Config
######################################################################

readonly PACKAGES_EXT=xz
readonly RELEASE_FILE="Release"
readonly RELEASE_FILE_GPG="Release.gpg"

######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}


SubBanner() {
  echo "----------------------------------------------------------------------"
  echo $*
  echo "----------------------------------------------------------------------"
}


Usage() {
  egrep "^#@" "${BASH_SOURCE[0]}" | cut --bytes=3-
}


DownloadOrCopyNonUniqueFilename() {
  # Use this function instead of DownloadOrCopy when the url uniquely
  # identifies the file, but the filename (excluding the directory)
  # does not.
  local url="$1"
  local dest="$2"

  local hash="$(echo "$url" | sha256sum | cut -d' ' -f1)"

  DownloadOrCopy "${url}" "${dest}.${hash}"
  # cp the file to prevent having to redownload it, but mv it to the
  # final location so that it's atomic.
  cp "${dest}.${hash}" "${dest}.$$"
  mv "${dest}.$$" "${dest}"
}

DownloadOrCopy() {
  if [ -f "$2" ] ; then
    echo "$2 already in place"
    return
  fi

  HTTP=0
  echo "$1" | grep -Eqs '^https?://' && HTTP=1
  if [ "$HTTP" = "1" ]; then
    SubBanner "downloading from $1 -> $2"
    # Appending the "$$" shell pid is necessary here to prevent concurrent
    # instances of sysroot-creator.sh from trying to write to the same file.
    local temp_file="${2}.partial.$$"
    # curl --retry doesn't retry when the page gives a 4XX error, so we need to
    # manually rerun.
    for i in {1..10}; do
      # --create-dirs is added in case there are slashes in the filename, as can
      # happen with the "debian/security" release class.
      local http_code=$(curl -L "$1" --create-dirs -o "${temp_file}" \
                        -w "%{http_code}")
      if [ ${http_code} -eq 200 ]; then
        break
      fi
      echo "Bad HTTP code ${http_code} when downloading $1"
      rm -f "${temp_file}"
      sleep $i
    done
    if [ ! -f "${temp_file}" ]; then
      exit 1
    fi
    mv "${temp_file}" $2
  else
    SubBanner "copying from $1"
    cp "$1" "$2"
  fi
}

SetEnvironmentVariables() {
  case $ARCH in
    amd64)
      TRIPLE=x86_64-linux-gnu
      DEBIAN_PACKAGES_ARCH="${DEBIAN_PACKAGES_AMD64}"
      ;;
    i386)
      TRIPLE=i386-linux-gnu
      DEBIAN_PACKAGES_ARCH="${DEBIAN_PACKAGES_I386}"
      ;;
    armhf)
      TRIPLE=arm-linux-gnueabihf
      DEBIAN_PACKAGES_ARCH="${DEBIAN_PACKAGES_ARMHF}"
      ;;
    arm64)
      TRIPLE=aarch64-linux-gnu
      DEBIAN_PACKAGES_ARCH="${DEBIAN_PACKAGES_ARM64}"
      ;;
    armel)
      TRIPLE=arm-linux-gnueabi
      DEBIAN_PACKAGES_ARCH="${DEBIAN_PACKAGES_ARMEL}"
      ;;
    mipsel)
      TRIPLE=mipsel-linux-gnu
      DEBIAN_PACKAGES_ARCH="${DEBIAN_PACKAGES_MIPSEL}"
      ;;
    mips64el)
      TRIPLE=mips64el-linux-gnuabi64
      DEBIAN_PACKAGES_ARCH="${DEBIAN_PACKAGES_MIPS64EL}"
      ;;
    *)
      echo "ERROR: Unsupported architecture: $ARCH"
      Usage
      exit 1
      ;;
  esac
}

# some sanity checks to make sure this script is run from the right place
# with the right tools
SanityCheck() {
  Banner "Sanity Checks"

  local chrome_dir=$(cd "${SCRIPT_DIR}/../../.." && pwd)
  BUILD_DIR="${chrome_dir}/out/sysroot-build/${RELEASE}"
  mkdir -p ${BUILD_DIR}
  echo "Using build directory: ${BUILD_DIR}"

  for tool in ${REQUIRED_TOOLS} ; do
    if ! which ${tool} > /dev/null ; then
      echo "Required binary $tool not found."
      echo "Exiting."
      exit 1
    fi
  done

  # This is where the staging sysroot is.
  INSTALL_ROOT="${BUILD_DIR}/${RELEASE}_${ARCH}_staging"
  TARBALL="${BUILD_DIR}/${DISTRO}_${RELEASE}_${ARCH}_sysroot.tar.xz"

  if ! mkdir -p "${INSTALL_ROOT}" ; then
    echo "ERROR: ${INSTALL_ROOT} can't be created."
    exit 1
  fi
}


ChangeDirectory() {
  # Change directory to where this script is.
  cd ${SCRIPT_DIR}
}


ClearInstallDir() {
  Banner "Clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  Banner "Creating tarball ${TARBALL}"
  tar -I "xz -9 -T0" -cf ${TARBALL} -C ${INSTALL_ROOT} .
}

ExtractPackageXz() {
  local src_file="$1"
  local dst_file="$2"
  local repo="$3"
  xzcat "${src_file}" | egrep '^(Package:|Filename:|SHA256:) ' |
    sed "s|Filename: |Filename: ${repo}|" > "${dst_file}"
}

GeneratePackageListDistRepo() {
  local arch="$1"
  local repo="$2"
  local dist="$3"
  local repo_name="$4"

  local tmp_package_list="${BUILD_DIR}/Packages.${dist}_${repo_name}_${arch}"
  local repo_basedir="${repo}/dists/${dist}"
  local package_list="${BUILD_DIR}/Packages.${dist}_${repo_name}_${arch}.${PACKAGES_EXT}"
  local package_file_arch="${repo_name}/binary-${arch}/Packages.${PACKAGES_EXT}"
  local package_list_arch="${repo_basedir}/${package_file_arch}"

  DownloadOrCopyNonUniqueFilename "${package_list_arch}" "${package_list}"
  VerifyPackageListing "${package_file_arch}" "${package_list}" ${repo} ${dist}
  ExtractPackageXz "${package_list}" "${tmp_package_list}" ${repo}
  cat "${tmp_package_list}" | ./merge-package-lists.py "${list_base}"
}

GeneratePackageListDist() {
  local arch="$1"
  set -- $2
  local repo="$1"
  local dist="$2"
  shift 2
  while (( "$#" )); do
    GeneratePackageListDistRepo "$arch" "$repo" "$dist" "$1"
    shift
  done
}

GeneratePackageList() {
  local output_file="$1"
  local arch="$2"
  local packages="$3"

  local list_base="${BUILD_DIR}/Packages.${RELEASE}_${arch}"
  > "${list_base}"  # Create (or truncate) a zero-length file.
  printf '%s\n' "${APT_SOURCES_LIST[@]}" | while read source; do
    GeneratePackageListDist "${arch}" "${source}"
  done

  GeneratePackageListImpl "${list_base}" "${output_file}" \
    "${DEBIAN_PACKAGES} ${packages}"
}

StripChecksumsFromPackageList() {
  local package_file="$1"
  sed -i 's/ [a-f0-9]\{64\}$//' "$package_file"
}

######################################################################
#
######################################################################

HacksAndPatches() {
  Banner "Misc Hacks & Patches"

  # Remove an unnecessary dependency on qtchooser.
  rm "${INSTALL_ROOT}/usr/lib/${TRIPLE}/qt-default/qtchooser/default.conf"

  # libxcomposite1 is missing a symbols file.
  cp "${SCRIPT_DIR}/libxcomposite1-symbols" \
    "${INSTALL_ROOT}/debian/libxcomposite1/DEBIAN/symbols"

  # __GLIBC_MINOR__ is used as a feature test macro.  Replace it with the
  # earliest supported version of glibc (2.26, obtained from the oldest glibc
  # version in //chrome/installer/linux/debian/dist_packag_versions.json and
  # //chrome/installer/linux/rpm/dist_package_provides.json).
  local usr_include="${INSTALL_ROOT}/usr/include"
  local features_h="${usr_include}/features.h"
  sed -i 's|\(#define\s\+__GLIBC_MINOR__\)|\1 26 //|' "${features_h}"

  # fcntl64() was introduced in glibc 2.28.  Make sure to use fcntl() instead.
  local fcntl_h="${INSTALL_ROOT}/usr/include/fcntl.h"
  sed -i '{N; s/#ifndef __USE_FILE_OFFSET64\(\nextern int fcntl\)/#if 1\1/}' \
      "${fcntl_h}"

  # Do not use pthread_cond_clockwait as it was introduced in glibc 2.30.
  local cppconfig_h="${usr_include}/${TRIPLE}/c++/10/bits/c++config.h"
  sed -i 's|\(#define\s\+_GLIBCXX_USE_PTHREAD_COND_CLOCKWAIT\)|// \1|' \
    "${cppconfig_h}"

  # Include limits.h in stdlib.h to fix an ODR issue
  # (https://sourceware.org/bugzilla/show_bug.cgi?id=30516)
  local stdlib_h="${usr_include}/stdlib.h"
  sed -i '/#include <stddef.h>/a #include <limits.h>' "${stdlib_h}"

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_LIBDIR internally
  SubBanner "Move pkgconfig scripts"
  mkdir -p ${INSTALL_ROOT}/usr/lib/pkgconfig
  mv ${INSTALL_ROOT}/usr/lib/${TRIPLE}/pkgconfig/* \
      ${INSTALL_ROOT}/usr/lib/pkgconfig

  # Avoid requiring unsupported glibc versions.
  "${SCRIPT_DIR}/reversion_glibc.py" \
    "${INSTALL_ROOT}/lib/${TRIPLE}/libc.so.6"
  "${SCRIPT_DIR}/reversion_glibc.py" \
    "${INSTALL_ROOT}/lib/${TRIPLE}/libm.so.6"
  "${SCRIPT_DIR}/reversion_glibc.py" \
    "${INSTALL_ROOT}/lib/${TRIPLE}/libcrypt.so.1"
}

InstallIntoSysroot() {
  Banner "Install Libs And Headers Into Jail"

  mkdir -p ${BUILD_DIR}/debian-packages
  # The /debian directory is an implementation detail that's used to cd into
  # when running dpkg-shlibdeps.
  mkdir -p ${INSTALL_ROOT}/debian
  # An empty control file is necessary to run dpkg-shlibdeps.
  touch ${INSTALL_ROOT}/debian/control
  while (( "$#" )); do
    local file="$1"
    local package="${BUILD_DIR}/debian-packages/${file##*/}"
    shift
    local sha256sum="$1"
    shift
    if [ "${#sha256sum}" -ne "64" ]; then
      echo "Bad sha256sum from package list"
      exit 1
    fi

    Banner "Installing $(basename ${file})"
    DownloadOrCopy ${file} ${package}
    if [ ! -s "${package}" ] ; then
      echo
      echo "ERROR: bad package ${package}"
      exit 1
    fi
    echo "${sha256sum}  ${package}" | sha256sum --quiet -c

    SubBanner "Extracting to ${INSTALL_ROOT}"
    dpkg-deb -x ${package} ${INSTALL_ROOT}

    base_package=$(dpkg-deb --field ${package} Package)
    mkdir -p ${INSTALL_ROOT}/debian/${base_package}/DEBIAN
    dpkg-deb -e ${package} ${INSTALL_ROOT}/debian/${base_package}/DEBIAN
  done

  # Prune /usr/share, leaving only pkgconfig, wayland, and wayland-protocols.
  ls -d ${INSTALL_ROOT}/usr/share/* | \
    grep -v "/\(pkgconfig\|wayland\|wayland-protocols\)$" | xargs rm -r
}


CleanupJailSymlinks() {
  Banner "Jail symlink cleanup"

  SAVEDPWD=$(pwd)
  cd ${INSTALL_ROOT}
  local libdirs="lib usr/lib"
  if [ -d lib64 ]; then
    libdirs="${libdirs} lib64"
  fi

  find $libdirs -type l -printf '%p %l\n' | while read link target; do
    # skip links with non-absolute paths
    echo "${target}" | grep -qs ^/ || continue
    echo "${link}: ${target}"
    # Relativize the symlink.
    prefix=$(echo "${link}" | sed -e 's/[^/]//g' | sed -e 's|/|../|g')
    ln -snfv "${prefix}${target}" "${link}"
  done

  failed=0
  while read link target; do
    # Make sure we catch new bad links.
    if [ ! -r "${link}" ]; then
      echo "ERROR: FOUND BAD LINK ${link}"
      ls -l ${link}
      failed=1
    fi
  done < <(find $libdirs -type l -printf '%p %l\n')
  if [ $failed -eq 1 ]; then
      exit 1
  fi
  cd "$SAVEDPWD"
}


VerifyLibraryDeps() {
  local find_dirs=(
    "${INSTALL_ROOT}/lib/"
    "${INSTALL_ROOT}/lib/${TRIPLE}/"
    "${INSTALL_ROOT}/usr/lib/${TRIPLE}/"
  )
  local needed_libs="$(
    find ${find_dirs[*]} -name "*\.so*" -type f -exec file {} \; | \
      grep ': ELF' | sed 's/^\(.*\): .*$/\1/' | xargs readelf -d | \
      grep NEEDED | sort | uniq | sed 's/^.*Shared library: \[\(.*\)\]$/\1/g')"
  local all_libs="$(find ${find_dirs[*]} -printf '%f\n')"
  # Ignore missing libdbus-1.so.0
  all_libs+="$(echo -e '\nlibdbus-1.so.0')"
  local missing_libs="$(grep -vFxf <(echo "${all_libs}") \
    <(echo "${needed_libs}"))"
  if [ ! -z "${missing_libs}" ]; then
    echo "Missing libraries:"
    echo "${missing_libs}"
    exit 1
  fi
}

BuildSysroot() {
  ClearInstallDir
  local package_file="generated_package_lists/${RELEASE}.${ARCH}"
  GeneratePackageList "${package_file}" $ARCH "${DEBIAN_PACKAGES_ARCH}"
  local files_and_sha256sums="$(cat ${package_file})"
  StripChecksumsFromPackageList "$package_file"
  InstallIntoSysroot ${files_and_sha256sums}
  HacksAndPatches
  CleanupJailSymlinks
  VerifyLibraryDeps
  CreateTarBall
}

UploadSysroot() {
  local sha=$(sha1sum "${TARBALL}" | awk '{print $1;}')
  set -x
  gsutil.py cp -a public-read "${TARBALL}" \
      "gs://chrome-linux-sysroot/toolchain/$sha/"
  set +x
}

#
# CheckForDebianGPGKeyring
#
#     Make sure the Debian GPG keys exist. Otherwise print a helpful message.
#
CheckForDebianGPGKeyring() {
  if [ ! -e "$KEYRING_FILE" ]; then
    echo "KEYRING_FILE not found: ${KEYRING_FILE}"
    echo "Debian GPG keys missing. Install the debian-archive-keyring package."
    exit 1
  fi
}

#
# VerifyPackageListing
#
#     Verifies the downloaded Packages.xz file has the right checksums.
#
VerifyPackageListing() {
  local file_path="$1"
  local output_file="$2"
  local repo="$3"
  local dist="$4"

  local repo_basedir="${repo}/dists/${dist}"
  local release_list="${repo_basedir}/${RELEASE_FILE}"
  local release_list_gpg="${repo_basedir}/${RELEASE_FILE_GPG}"

  local release_file="${BUILD_DIR}/${dist}-${RELEASE_FILE}"
  local release_file_gpg="${BUILD_DIR}/${dist}-${RELEASE_FILE_GPG}"

  CheckForDebianGPGKeyring

  DownloadOrCopyNonUniqueFilename ${release_list} ${release_file}
  DownloadOrCopyNonUniqueFilename ${release_list_gpg} ${release_file_gpg}
  echo "Verifying: ${release_file} with ${release_file_gpg}"
  set -x
  gpgv --keyring "${KEYRING_FILE}" "${release_file_gpg}" "${release_file}"
  set +x

  echo "Verifying: ${output_file}"
  local sha256sum=$(grep -E "${file_path}\$|:\$" "${release_file}" | \
    grep "SHA256:" -A 1 | xargs echo | awk '{print $2;}')

  if [ "${#sha256sum}" -ne "64" ]; then
    echo "Bad sha256sum from ${release_list}"
    exit 1
  fi

  echo "${sha256sum}  ${output_file}" | sha256sum --quiet -c
}

#
# GeneratePackageListImpl
#
#     Looks up package names in ${BUILD_DIR}/Packages and write list of URLs
#     to output file.
#
GeneratePackageListImpl() {
  local input_file="$1"
  local output_file="$2"
  echo "Updating: ${output_file} from ${input_file}"
  /bin/rm -f "${output_file}"
  shift
  shift
  local failed=0
  for pkg in $@ ; do
    local pkg_full=$(grep -A 1 " ${pkg}\$" "$input_file" | \
      egrep "pool/.*" | sed 's/.*Filename: //')
    if [ -z "${pkg_full}" ]; then
      echo "ERROR: missing package: $pkg"
      local failed=1
    else
      local sha256sum=$(grep -A 4 " ${pkg}\$" "$input_file" | \
        grep ^SHA256: | sed 's/^SHA256: //')
      if [ "${#sha256sum}" -ne "64" ]; then
        echo "Bad sha256sum from Packages"
        local failed=1
      fi
      echo $pkg_full $sha256sum >> "$output_file"
    fi
  done
  if [ $failed -eq 1 ]; then
    exit 1
  fi
  # sort -o does an in-place sort of this file
  sort "$output_file" -o "$output_file"
}

if [ $# -ne 2 ]; then
  Usage
  exit 1
else
  ChangeDirectory
  ARCH=$2
  SetEnvironmentVariables
  SanityCheck
  case "$1" in
    build)
      BuildSysroot
      ;;
    upload)
      UploadSysroot
      ;;
    *)
      echo "ERROR: Invalid command: $1"
      Usage
      exit 1
      ;;
  esac
fi
