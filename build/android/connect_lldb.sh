#!/bin/bash
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# A generic script used to attach to a running Chromium process and debug it.
# Most users should not use this directly, but one of the wrapper scripts like
# connect_lldb.sh_content_shell
#
# Use --help to print full usage instructions.
#

PROGNAME=$(basename "$0")
PROGDIR=$(dirname "$0")

# Force locale to C to allow recognizing output from subprocesses.
LC_ALL=C

# Location of Chromium-top-level sources.
CHROMIUM_SRC=$(cd "$PROGDIR"/../.. >/dev/null && pwd 2>/dev/null)

TMPDIR=
LLDB_SERVER_JOB_PIDFILE=
LLDB_SERVER_PID=
TARGET_LLDB_SERVER=
COMMAND_PREFIX=
COMMAND_SUFFIX=

clean_exit () {
  if [ "$TMPDIR" ]; then
    LLDB_SERVER_JOB_PID=$(cat $LLDB_SERVER_JOB_PIDFILE 2>/dev/null)
    if [ "$LLDB_SERVER_PID" ]; then
      log "Killing lldb-server process on-device: $LLDB_SERVER_PID"
      adb_shell kill $LLDB_SERVER_PID
    fi
    if [ "$LLDB_SERVER_JOB_PID" ]; then
      log "Killing background lldb-server process: $LLDB_SERVER_JOB_PID"
      kill -9 $LLDB_SERVER_JOB_PID >/dev/null 2>&1
      rm -f "$LLDB_SERVER_JOB_PIDFILE"
    fi
    if [ "$TARGET_LLDB_SERVER" ]; then
      log "Removing target lldb-server binary: $TARGET_LLDB_SERVER."
      "$ADB" shell "$COMMAND_PREFIX" rm "$TARGET_LLDB_SERVER" \
          "$TARGET_DOMAIN_SOCKET" "$COMMAND_SUFFIX" >/dev/null 2>&1
    fi
    log "Cleaning up: $TMPDIR"
    rm -rf "$TMPDIR"
  fi
  trap "" EXIT
  exit $1
}

# Ensure clean exit on Ctrl-C or normal exit.
trap "clean_exit 1" INT HUP QUIT TERM
trap "clean_exit \$?" EXIT

panic () {
  echo "ERROR: $@" >&2
  exit 1
}

fail_panic () {
  if [ $? != 0 ]; then panic "$@"; fi
}

log () {
  if [ "$VERBOSE" -gt 0 ]; then
    echo "$@"
  fi
}

DEFAULT_PULL_LIBS_DIR="/tmp/adb-lldb-support-$USER"

# NOTE: Allow wrapper scripts to set various default through ADB_LLDB_XXX
# environment variables. This is only for cosmetic reasons, i.e. to
# display proper default in the --help output.

# Allow wrapper scripts to set the program name through ADB_LLDB_PROGNAME
PROGNAME=${ADB_LLDB_PROGNAME:-$(basename "$0")}

ADB=
ATTACH_DELAY=1
HELP=
LLDB_INIT=
LLDB_SERVER=
NDK_DIR=
NO_PULL_LIBS=
PACKAGE_NAME=
PID=
PORT=
PROCESS_NAME=
PROGRAM_NAME="activity"
PULL_LIBS=
PULL_LIBS_DIR=
SU_PREFIX=
SYMBOL_DIR=
TARGET_ARCH=
TOOLCHAIN=
VERBOSE=0

for opt; do
  optarg=$(expr "x$opt" : 'x[^=]*=\(.*\)')
  case $opt in
    --adb=*) ADB=$optarg ;;
    --attach-delay=*) ATTACH_DELAY=$optarg ;;
    --device=*) export ANDROID_SERIAL=$optarg ;;
    --help|-h|-?) HELP=true ;;
    --lldb=*) LLDB=$optarg ;;
    --lldb-server=*) LLDB_SERVER=$optarg ;;
    --ndk-dir=*) NDK_DIR=$optarg ;;
    --no-pull-libs) NO_PULL_LIBS=true ;;
    --output-directory=*) CHROMIUM_OUTPUT_DIR=$optarg ;;
    --package-name=*) PACKAGE_NAME=$optarg ;;
    --pid=*) PID=$optarg ;;
    --port=*) PORT=$optarg ;;
    --process-name=*) PROCESS_NAME=$optarg ;;
    --program-name=*) PROGRAM_NAME=$optarg ;;
    --pull-libs) PULL_LIBS=true ;;
    --pull-libs-dir=*) PULL_LIBS_DIR=$optarg ;;
    --source=*) LLDB_INIT=$optarg ;;
    --su-prefix=*) SU_PREFIX=$optarg ;;
    --symbol-dir=*) SYMBOL_DIR=$optarg ;;
    --target-arch=*) TARGET_ARCH=$optarg ;;
    --toolchain=*) TOOLCHAIN=$optarg ;;
    --verbose) VERBOSE=$(( $VERBOSE + 1 )) ;;
    -*)
      panic "Unknown option $opt, see --help." >&2
      ;;
    *)
      if [ "$PACKAGE_NAME" ]; then
        panic "You can only provide a single package name as argument!\
 See --help."
      fi
      PACKAGE_NAME=$opt
      ;;
  esac
done

if [ "$HELP" ]; then
  if [ "$ADB_LLDB_PROGNAME" ]; then
    # Assume wrapper scripts all provide a default package name.
    cat <<EOF
Usage: $PROGNAME [options]

Attach lldb to a running Android $PROGRAM_NAME process.
EOF
  else
    # Assume this is a direct call to connect_lldb.sh
  cat <<EOF
Usage: $PROGNAME [options] [<package-name>]

Attach lldb to a running Android $PROGRAM_NAME process.

If provided, <package-name> must be the name of the Android application's
package name to be debugged. You can also use --package-name=<name> to
specify it.
EOF
  fi

  cat <<EOF

This script is used to debug a running $PROGRAM_NAME process.

This script needs several things to work properly. It will try to pick
them up automatically for you though:

   - target lldb-server binary
   - host lldb client, possibly a wrapper (e.g. lldb.sh)
   - directory with symbolic version of $PROGRAM_NAME's shared libraries.

You can also use --ndk-dir=<path> to specify an alternative NDK installation
directory.

The script tries to find the most recent version of the debug version of
shared libraries under one of the following directories:

  \$CHROMIUM_SRC/<out>/lib.unstripped/     (used by GN builds)

Where <out> is determined by CHROMIUM_OUTPUT_DIR, or --output-directory.

You can set the path manually via --symbol-dir.

The script tries to extract the target architecture from your target device,
but if this fails, will default to 'arm'. Use --target-arch=<name> to force
its value.

Otherwise, the script will complain, but you can use the --lldb-server,
--lldb and --symbol-lib options to specify everything manually.

An alternative to --lldb=<file> is to use --toolchain=<path> to specify
the path to the host target-specific cross-toolchain.

You will also need the 'adb' tool in your path. Otherwise, use the --adb
option. The script will complain if there is more than one device connected
and a device is not specified with either --device or ANDROID_SERIAL).

The first time you use it on a device, the script will pull many system
libraries required by the process into a temporary directory. This
is done to strongly improve the debugging experience, like allowing
readable thread stacks and more. The libraries are copied to the following
directory by default:

  $DEFAULT_PULL_LIBS_DIR/

But you can use the --pull-libs-dir=<path> option to specify an
alternative. The script can detect when you change the connected device,
and will re-pull the libraries only in this case. You can however force it
with the --pull-libs option.

Any local .lldb-init script will be ignored, but it is possible to pass a
lldb command script with the --source=<file> option. Note that its commands
will be passed to lldb after the remote connection and library symbol
loading have completed.

Valid options:
  --help|-h|-?          Print this message.
  --verbose             Increase verbosity.

  --symbol-dir=<path>   Specify directory with symbol shared libraries.
  --output-directory=<path> Specify the output directory (e.g. "out/Debug").
  --package-name=<name> Specify package name (alternative to 1st argument).
  --program-name=<name> Specify program name (cosmetic only).
  --process-name=<name> Specify process name to attach to (uses package-name
                        if not passsed).
  --pid=<pid>           Specify application process pid.
  --attach-delay=<num>  Seconds to wait for lldb-server to attach to the
                        remote process before starting lldb. Default 1.
                        <num> may be a float if your sleep(1) supports it.
  --source=<file>       Specify extra LLDB init script.

  --lldb-server=<file>    Specify target lldb-server binary.
  --lldb=<file>          Specify host lldb client binary.
  --target-arch=<name>  Specify NDK target arch.
  --adb=<file>          Specify host ADB binary.
  --device=<file>       ADB device serial to use (-s flag).
  --port=<port>         Specify the tcp port to use.

  --su-prefix=<prefix>  Prepend <prefix> to 'adb shell' commands that are
                        run by this script. This can be useful to use
                        the 'su' program on rooted production devices.
                        e.g. --su-prefix="su -c"

  --pull-libs           Force system libraries extraction.
  --no-pull-libs        Do not extract any system library.
  --libs-dir=<path>     Specify system libraries extraction directory.

EOF
  exit 0
fi

if [ -z "$PACKAGE_NAME" ]; then
  panic "Please specify a package name on the command line. See --help."
fi

if [[ -z "$SYMBOL_DIR" && -z "$CHROMIUM_OUTPUT_DIR" ]]; then
  if [[ -e "build.ninja" ]]; then
    CHROMIUM_OUTPUT_DIR=$PWD
  else
    panic "Please specify an output directory by using one of:
       --output-directory=out/Debug
       CHROMIUM_OUTPUT_DIR=out/Debug
       Setting working directory to an output directory.
       See --help."
   fi
fi

if ls *.so >/dev/null 2>&1; then
  panic ".so files found in your working directory. These will conflict with" \
      "library lookup logic. Change your working directory and try again."
fi

# Detects the build type and symbol directory. This is done by finding
# the most recent sub-directory containing debug shared libraries under
# $CHROMIUM_OUTPUT_DIR.
# Out: nothing, but this sets SYMBOL_DIR
detect_symbol_dir () {
  # GN places unstripped libraries under out/lib.unstripped
  local PARENT_DIR="$CHROMIUM_OUTPUT_DIR"
  if [[ ! -e "$PARENT_DIR" ]]; then
    PARENT_DIR="$CHROMIUM_SRC/$PARENT_DIR"
  fi
  SYMBOL_DIR="$PARENT_DIR/lib.unstripped"
  if [[ -z "$(ls "$SYMBOL_DIR"/lib*.so 2>/dev/null)" ]]; then
    SYMBOL_DIR="$PARENT_DIR/lib"
    if [[ -z "$(ls "$SYMBOL_DIR"/lib*.so 2>/dev/null)" ]]; then
      panic "Could not find any symbols under \
$PARENT_DIR/lib{.unstripped}. Please build the program first!"
    fi
  fi
  log "Auto-config: --symbol-dir=$SYMBOL_DIR"
}

if [ -z "$SYMBOL_DIR" ]; then
  detect_symbol_dir
elif [[ -z "$(ls "$SYMBOL_DIR"/lib*.so 2>/dev/null)" ]]; then
  panic "Could not find any symbols under $SYMBOL_DIR"
fi

if [ -z "$NDK_DIR" ]; then
  ANDROID_NDK_ROOT=$(PYTHONPATH=$CHROMIUM_SRC/build/android python3 -c \
    'from pylib.constants import ANDROID_NDK_ROOT; print(ANDROID_NDK_ROOT,)')
else
  if [ ! -d "$NDK_DIR" ]; then
    panic "Invalid directory: $NDK_DIR"
  fi
  if [ ! -d "$NDK_DIR/toolchains" ]; then
    panic "Not a valid NDK directory: $NDK_DIR"
  fi
  ANDROID_NDK_ROOT=$NDK_DIR
fi

if [ "$LLDB_INIT" -a ! -f "$LLDB_INIT" ]; then
  panic "Unknown --source file: $LLDB_INIT"
fi

# Checks that ADB is in our path
if [ -z "$ADB" ]; then
  ADB=$(which adb 2>/dev/null)
  if [ -z "$ADB" ]; then
    panic "Can't find 'adb' tool in your path. Install it or use \
--adb=<file>"
  fi
  log "Auto-config: --adb=$ADB"
fi

# Checks that it works minimally
ADB_VERSION=$($ADB version 2>/dev/null)
echo "$ADB_VERSION" | fgrep -q -e "Android Debug Bridge"
if [ $? != 0 ]; then
  panic "Your 'adb' tool seems invalid, use --adb=<file> to specify a \
different one: $ADB"
fi

# If there are more than one device connected, and ANDROID_SERIAL is not
# defined, prints an error message.
NUM_DEVICES_PLUS2=$($ADB devices 2>/dev/null | wc -l)
if [ "$NUM_DEVICES_PLUS2" -gt 3 -a -z "$ANDROID_SERIAL" ]; then
  echo "ERROR: There is more than one Android device connected to ADB."
  echo "Please define ANDROID_SERIAL to specify which one to use."
  exit 1
fi

# Runs a command through adb shell, strip the extra \r from the output
# and return the correct status code to detect failures. This assumes
# that the adb shell command prints a final \n to stdout.
# $1+: command to run
# Out: command's stdout
# Return: command's status
# Note: the command's stderr is lost
# Info: In Python would be done via DeviceUtils.RunShellCommand().
adb_shell () {
  local TMPOUT="$(mktemp)"
  local LASTLINE RET
  local ADB=${ADB:-adb}

  # The weird sed rule is to strip the final \r on each output line
  # Since 'adb shell' never returns the command's proper exit/status code,
  # we force it to print it as '%%<status>' in the temporary output file,
  # which we will later strip from it.
  $ADB shell $@ ";" echo "%%\$?" 2>/dev/null | \
      sed -e 's![[:cntrl:]]!!g' > $TMPOUT
  # Get last line in log, which contains the exit code from the command
  LASTLINE=$(sed -e '$!d' $TMPOUT)
  # Extract the status code from the end of the line, which must
  # be '%%<code>'.
  RET=$(echo "$LASTLINE" | \
    awk '{ if (match($0, "%%[0-9]+$")) { print substr($0,RSTART+2); } }')
  # Remove the status code from the last line. Note that this may result
  # in an empty line.
  LASTLINE=$(echo "$LASTLINE" | \
    awk '{ if (match($0, "%%[0-9]+$")) { print substr($0,1,RSTART-1); } }')
  # The output itself: all lines except the status code.
  sed -e '$d' $TMPOUT && printf "%s" "$LASTLINE"
  # Remove temp file.
  rm -f $TMPOUT
  # Exit with the appropriate status.
  return $RET
}

# Finds the target architecture from a local shared library.
# This returns an NDK-compatible architecture name.
# Out: NDK Architecture name, or empty string.
get_gn_target_arch () {
  # ls prints a broken pipe error when there are a lot of libs.
  local RANDOM_LIB=$(ls "$SYMBOL_DIR"/lib*.so 2>/dev/null| head -n1)
  local SO_DESC=$(file $RANDOM_LIB)
  case $SO_DESC in
    *32-bit*ARM,*) echo "arm";;
    *64-bit*ARM,*) echo "arm64";;
    *64-bit*aarch64,*) echo "arm64";;
    *32-bit*Intel,*) echo "x86";;
    *x86-64,*) echo "x86_64";;
    *32-bit*MIPS,*) echo "mips";;
    *) echo "";
  esac
}

if [ -z "$TARGET_ARCH" ]; then
  TARGET_ARCH=$(get_gn_target_arch)
  if [ -z "$TARGET_ARCH" ]; then
    TARGET_ARCH=arm
  fi
  log "Auto-config: --arch=$TARGET_ARCH"
else
  # Nit: accept Chromium's 'ia32' as a valid target architecture. This
  # script prefers the NDK 'x86' name instead because it uses it to find
  # NDK-specific files (host lldb) with it.
  if [ "$TARGET_ARCH" = "ia32" ]; then
    TARGET_ARCH=x86
    log "Auto-config: --arch=$TARGET_ARCH  (equivalent to ia32)"
  fi
fi

# Translates GN target architecure to NDK subdirectory name.
# $1: GN target architecture.
# Out: NDK subdirectory name.
get_ndk_arch_dir () {
  case "$1" in
    arm64) echo "aarch64";;
    x86) echo "i386";;
    *) echo "$1";
  esac
}

# Detects the NDK system name, i.e. the name used to identify the host.
# out: NDK system name (e.g. 'linux' or 'darwin')
get_ndk_host_system () {
  local HOST_OS
  if [ -z "$NDK_HOST_SYSTEM" ]; then
    HOST_OS=$(uname -s)
    case $HOST_OS in
      Linux) NDK_HOST_SYSTEM=linux;;
      Darwin) NDK_HOST_SYSTEM=darwin;;
      *) panic "You can't run this script on this system: $HOST_OS";;
    esac
  fi
  echo "$NDK_HOST_SYSTEM"
}

# Detects the NDK host architecture name.
# out: NDK arch name (e.g. 'x86_64')
get_ndk_host_arch () {
  echo "x86_64"
}

# $1: NDK install path.
get_ndk_host_lldb_client() {
  local NDK_DIR="$1"
  local HOST_OS=$(get_ndk_host_system)
  local HOST_ARCH=$(get_ndk_host_arch)
  echo "$NDK_DIR/toolchains/llvm/prebuilt/$HOST_OS-$HOST_ARCH/bin/lldb.sh"
}

# $1: NDK install path.
# $2: target architecture.
get_ndk_lldb_server () {
  local NDK_DIR="$1"
  local ARCH=$2
  local HOST_OS=$(get_ndk_host_system)
  local HOST_ARCH=$(get_ndk_host_arch)
  local NDK_ARCH_DIR=$(get_ndk_arch_dir "$ARCH")
  local i
  # For lldb-server is under lib64/ for r25, and lib/ for r26+.
  for i in "lib64" "lib"; do
    local RET=$(realpath -m $NDK_DIR/toolchains/llvm/prebuilt/$HOST_OS-$HOST_ARCH/$i/clang/*/lib/linux/$NDK_ARCH_DIR/lldb-server)
    if [ -e "$RET" ]; then
      echo $RET
      return 0
    fi
  done
  return 1
}

# Find host LLDB client binary
if [ -z "$LLDB" ]; then
  LLDB=$(get_ndk_host_lldb_client "$ANDROID_NDK_ROOT")
  if [ -z "$LLDB" ]; then
    panic "Can't find Android lldb client in your path, check your \
--toolchain or --lldb path."
  fi
  log "Host lldb client: $LLDB"
fi

# Find lldb-server binary, we will later push it to /data/local/tmp
# This ensures that both lldb-server and $LLDB talk the same binary protocol,
# otherwise weird problems will appear.
if [ -z "$LLDB_SERVER" ]; then
  LLDB_SERVER=$(get_ndk_lldb_server "$ANDROID_NDK_ROOT" "$TARGET_ARCH")
  if [ -z "$LLDB_SERVER" ]; then
    panic "Can't find NDK lldb-server binary. use --lldb-server to specify \
valid one!"
  fi
  log "Auto-config: --lldb-server=$LLDB_SERVER"
fi

# A unique ID for this script's session. This needs to be the same in all
# sub-shell commands we're going to launch, so take the PID of the launcher
# process.
TMP_ID=$$

# Temporary directory, will get cleaned up on exit.
TMPDIR=/tmp/$USER-adb-lldb-tmp-$TMP_ID
mkdir -p "$TMPDIR" && rm -rf "$TMPDIR"/*

LLDB_SERVER_JOB_PIDFILE="$TMPDIR"/lldb-server-$TMP_ID.pid

# Returns the timestamp of a given file, as number of seconds since epoch.
# $1: file path
# Out: file timestamp
get_file_timestamp () {
  stat -c %Y "$1" 2>/dev/null
}

# Allow several concurrent debugging sessions
APP_DATA_DIR=$(adb_shell run-as $PACKAGE_NAME /system/bin/sh -c pwd)
if [ $? != 0 ]; then
  echo "Failed to run-as $PACKAGE_NAME, is the app debuggable?"
  APP_DATA_DIR=$(adb_shell dumpsys package $PACKAGE_NAME | \
    sed -ne 's/^ \+dataDir=//p' | head -n1)
fi
log "App data dir: $APP_DATA_DIR"
TARGET_LLDB_SERVER="$APP_DATA_DIR/lldb-server-adb-lldb-$TMP_ID"
TMP_TARGET_LLDB_SERVER=/data/local/tmp/lldb-server-adb-lldb-$TMP_ID

# Select correct app_process for architecture.
case $TARGET_ARCH in
      arm|x86|mips) LLDBEXEC=app_process32;;
      arm64|x86_64) LLDBEXEC=app_process64; SUFFIX_64_BIT=64;;
      *) panic "Unknown app_process for architecture!";;
esac

# Default to app_process if bit-width specific process isn't found.
adb_shell ls /system/bin/$LLDBEXEC > /dev/null
if [ $? != 0 ]; then
    LLDBEXEC=app_process
fi

# Detect AddressSanitizer setup on the device. In that case app_process is a
# script, and the real executable is app_process.real.
LLDBEXEC_ASAN=app_process.real
adb_shell ls /system/bin/$LLDBEXEC_ASAN > /dev/null
if [ $? == 0 ]; then
    LLDBEXEC=$LLDBEXEC_ASAN
fi

ORG_PULL_LIBS_DIR=$PULL_LIBS_DIR
if [[ -n "$ANDROID_SERIAL" ]]; then
  DEFAULT_PULL_LIBS_DIR="$DEFAULT_PULL_LIBS_DIR/$ANDROID_SERIAL-$SUFFIX_64_BIT"
fi
PULL_LIBS_DIR=${PULL_LIBS_DIR:-$DEFAULT_PULL_LIBS_DIR}

HOST_FINGERPRINT=
DEVICE_FINGERPRINT=$(adb_shell getprop ro.build.fingerprint)
[[ "$DEVICE_FINGERPRINT" ]] || panic "Failed to get the device fingerprint"
log "Device build fingerprint: $DEVICE_FINGERPRINT"

if [ ! -f "$PULL_LIBS_DIR/build.fingerprint" ]; then
  log "Auto-config: --pull-libs  (no cached libraries)"
  PULL_LIBS=true
else
  HOST_FINGERPRINT=$(< "$PULL_LIBS_DIR/build.fingerprint")
  log "Host build fingerprint:   $HOST_FINGERPRINT"
  if [ "$HOST_FINGERPRINT" == "$DEVICE_FINGERPRINT" ]; then
    log "Auto-config: --no-pull-libs (fingerprint match)"
    NO_PULL_LIBS=true
  else
    log "Auto-config: --pull-libs  (fingerprint mismatch)"
    PULL_LIBS=true
  fi
fi

# Get the PID from the first argument or else find the PID of the
# browser process (or the process named by $PROCESS_NAME).
if [ -z "$PID" ]; then
  if [ -z "$PROCESS_NAME" ]; then
    PROCESS_NAME=$PACKAGE_NAME
  fi
  if [ -z "$PID" ]; then
    PID=$(adb_shell ps | \
          awk '$9 == "'$PROCESS_NAME'" { print $2; }' | head -1)
  fi
  if [ -z "$PID" ]; then
    panic "Can't find application process PID."
  fi
  log "Found process PID: $PID"
fi

# Determine if 'adb shell' runs as root or not.
# If so, we can launch lldb-server directly, otherwise, we have to
# use run-as $PACKAGE_NAME ..., which requires the package to be debuggable.
#
if [ "$SU_PREFIX" ]; then
  # Need to check that this works properly.
  SU_PREFIX_TEST_LOG=$TMPDIR/su-prefix.log
  adb_shell $SU_PREFIX \"echo "foo"\" > $SU_PREFIX_TEST_LOG 2>&1
  if [ $? != 0 -o "$(cat $SU_PREFIX_TEST_LOG)" != "foo" ]; then
    echo "ERROR: Cannot use '$SU_PREFIX' as a valid su prefix:"
    echo "$ adb shell $SU_PREFIX \"echo foo\""
    cat $SU_PREFIX_TEST_LOG
    exit 1
  fi
  COMMAND_PREFIX="$SU_PREFIX \""
  COMMAND_SUFFIX="\""
else
  SHELL_UID=$("$ADB" shell cat /proc/self/status | \
              awk '$1 == "Uid:" { print $2; }')
  log "Shell UID: $SHELL_UID"
  if [ "$SHELL_UID" != 0 -o -n "$NO_ROOT" ]; then
    COMMAND_PREFIX="run-as $PACKAGE_NAME"
    COMMAND_SUFFIX=
  else
    COMMAND_PREFIX=
    COMMAND_SUFFIX=
  fi
fi
log "Command prefix: '$COMMAND_PREFIX'"
log "Command suffix: '$COMMAND_SUFFIX'"

mkdir -p "$PULL_LIBS_DIR"
fail_panic "Can't create --libs-dir directory: $PULL_LIBS_DIR"

# Pull device's system libraries that are mapped by our process.
# Pulling all system libraries is too long, so determine which ones
# we need by looking at /proc/$PID/maps instead
if [ "$PULL_LIBS" -a -z "$NO_PULL_LIBS" ]; then
  echo "Extracting system libraries into: $PULL_LIBS_DIR"
  MAPPINGS=$(adb_shell $COMMAND_PREFIX cat /proc/$PID/maps $COMMAND_SUFFIX)
  if [ $? != 0 ]; then
    echo "ERROR: Could not list process's memory mappings."
    if [ "$SU_PREFIX" ]; then
      panic "Are you sure your --su-prefix is correct?"
    else
      panic "Use --su-prefix if the application is not debuggable."
    fi
  fi
  # Remove the fingerprint file in case pulling one of the libs fails.
  rm -f "$PULL_LIBS_DIR/build.fingerprint"
  SYSTEM_LIBS=$(echo "$MAPPINGS" | \
      awk '$6 ~ /\/(system|apex|vendor)\/.*\.so$/ { print $6; }' | sort -u)
  for SYSLIB in /system/bin/linker$SUFFIX_64_BIT $SYSTEM_LIBS; do
    echo "Pulling from device: $SYSLIB"
    DST_FILE=$PULL_LIBS_DIR$SYSLIB
    DST_DIR=$(dirname "$DST_FILE")
    mkdir -p "$DST_DIR" && "$ADB" pull $SYSLIB "$DST_FILE" 2>/dev/null
    fail_panic "Could not pull $SYSLIB from device !?"
  done
  echo "Writing the device fingerprint"
  echo "$DEVICE_FINGERPRINT" > "$PULL_LIBS_DIR/build.fingerprint"
fi

# Pull the app_process binary from the device.
log "Pulling $LLDBEXEC from device"
"$ADB" pull /system/bin/$LLDBEXEC "$TMPDIR"/$LLDBEXEC &>/dev/null
fail_panic "Could not retrieve $LLDBEXEC from the device!"

# Find all the sub-directories of $PULL_LIBS_DIR, up to depth 4
# so we can add them to target.exec-search-paths later.
SOLIB_DIRS=$(find $PULL_LIBS_DIR -mindepth 1 -maxdepth 4 -type d | \
             grep -v "^$" | tr '\n' ' ')

# Applications with minSdkVersion >= 24 will have their data directories
# created with rwx------ permissions, preventing adbd from forwarding to
# the lldb-server socket.
adb_shell $COMMAND_PREFIX chmod a+x $APP_DATA_DIR $COMMAND_SUFFIX

# Push lldb-server to the device
log "Pushing lldb-server $LLDB_SERVER to $TARGET_LLDB_SERVER"
"$ADB" push $LLDB_SERVER $TMP_TARGET_LLDB_SERVER >/dev/null && \
    adb_shell $COMMAND_PREFIX cp $TMP_TARGET_LLDB_SERVER $TARGET_LLDB_SERVER $COMMAND_SUFFIX && \
    adb_shell rm $TMP_TARGET_LLDB_SERVER
fail_panic "Could not copy lldb-server to the device!"

if [ -z "$PORT" ]; then
  # Random port to allow multiple concurrent sessions.
  PORT=$(( $RANDOM % 1000 + 5039 ))
fi
HOST_PORT=$PORT
TARGET_DOMAIN_SOCKET=$APP_DATA_DIR/lldb-socket-$HOST_PORT

# Setup network redirection
log "Setting network redirection (host:$HOST_PORT -> device:$TARGET_DOMAIN_SOCKET)"
"$ADB" forward tcp:$HOST_PORT localfilesystem:$TARGET_DOMAIN_SOCKET
fail_panic "Could not setup network redirection from \
host:localhost:$HOST_PORT to device:$TARGET_DOMAIN_SOCKET"

# Start lldb-server in the background
# Note that using run-as requires the package to be debuggable.
#
# If not, this will fail horribly. The alternative is to run the
# program as root, which requires of course root privileges.
# Maybe we should add a --root option to enable this?

for i in 1 2; do
  log "Starting lldb-server in the background:"
  LLDB_SERVER_LOG=$TMPDIR/lldb-server-$TMP_ID.log
  log "adb shell $COMMAND_PREFIX $TARGET_LLDB_SERVER g \
    $TARGET_DOMAIN_SOCKET \
    --attach $PID $COMMAND_SUFFIX"
  "$ADB" shell $COMMAND_PREFIX $TARGET_LLDB_SERVER g \
    $TARGET_DOMAIN_SOCKET \
    --attach $PID $COMMAND_SUFFIX > $LLDB_SERVER_LOG 2>&1 &
  LLDB_SERVER_JOB_PID=$!
  LLDB_SERVER_PID=$(adb_shell $COMMAND_PREFIX pidof $(basename $TARGET_LLDB_SERVER))
  echo "$LLDB_SERVER_JOB_PID" > $LLDB_SERVER_JOB_PIDFILE
  log "background job pid: $LLDB_SERVER_JOB_PID"

  # Sleep to allow lldb-server to attach to the remote process and be
  # ready to connect to.
  log "Sleeping ${ATTACH_DELAY}s to ensure lldb-server is alive"
  sleep "$ATTACH_DELAY"
  log "Job control: $(jobs -l)"
  STATE=$(jobs -l | awk '$2 == "'$LLDB_SERVER_JOB_PID'" { print $3; }')
  if [ "$STATE" != "Running" ]; then
    pid_msg=$(grep "is already traced by process" $LLDB_SERVER_LOG 2>/dev/null)
    if [[ -n "$pid_msg" ]]; then
      old_pid=${pid_msg##* }
      old_pid=${old_pid//[$'\r\n']}  # Trim trailing \r.
      echo "Killing previous lldb-server process (pid=$old_pid)"
      adb_shell $COMMAND_PREFIX kill -9 $old_pid $COMMAND_SUFFIX
      continue
    fi
    echo "ERROR: lldb-server either failed to run or attach to PID $PID!"
    echo "Here is the output from lldb-server (also try --verbose for more):"
    echo "===== lldb-server.log start ====="
    cat $LLDB_SERVER_LOG
    echo ="===== lldb-server.log end ======"
    exit 1
  fi
  break
done

# Generate a file containing useful LLDB initialization commands
readonly COMMANDS=$TMPDIR/lldb.init
log "Generating LLDB initialization commands file: $COMMANDS"
cat > "$COMMANDS" <<EOF
settings append target.exec-search-paths $SYMBOL_DIR $SOLIB_DIRS $PULL_LIBS_DIR
settings set target.source-map ../.. $CHROMIUM_SRC
target create '$TMPDIR/$LLDBEXEC'
target modules search-paths add / $TMPDIR/$LLDBEXEC/
script print("Connecting to :$HOST_PORT... (symbol load can take a while)")
gdb-remote $HOST_PORT
EOF

if [ "$LLDB_INIT" ]; then
  cat "$LLDB_INIT" >> "$COMMANDS"
fi

if [ "$VERBOSE" -gt 0 ]; then
  echo "### START $COMMANDS"
  cat "$COMMANDS"
  echo "### END $COMMANDS"
fi

log "Launching lldb client: $LLDB $LLDB_ARGS --source $COMMANDS"
echo "Server log: $LLDB_SERVER_LOG"
$LLDB $LLDB_ARGS --source "$COMMANDS"
