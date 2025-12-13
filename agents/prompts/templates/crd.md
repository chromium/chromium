# Chrome Remote Desktop Instructions

Instructions that are relevant when working with the Chrome Remote Desktop (aka.
CRD and chromoting) host binaries, or code located in //remoting.

While code for the Chrome Remote Desktop host lives in the Chromium repo (this
repo), it is mostly an independent product, separate from Chrome, except for
ChromeOS, where the IT2ME native messaging host is part of the Chrome binary.

IMPORTANT: When making suggestions for CRD code changes, make sure they are
actually relevant to CRD. For example, code changes outside of //remoting
usually aren't relevant.

## Overview

### ME2ME and IT2ME

Remote access (ME2ME) refers to a feature/mode of CRD where:

*   The user sets up Me2Me once on the host machine. After that it continues to
    run as a daemon process, surviving logouts and reboots.
*   Chrome need not be running in order for the host process to be active.
    Specifically, it is possible to connect as soon as the computer has booted
    up, before anyone is logged on.

Remote support (IT2ME) refers to a feature/mode of CRD where:

*   A user is required at the Host computer to "invite" the Viewer computer to
    connect. This is done via a short-lived numerical code displayed at the Host
    and relayed over the phone to the Viewer.
*   Sessions are paused after 30 minutes, at which point the Host user must
    explicitly approve continuation. The Host process runs as a Native Messaging
    binary, with no privileged access to the system.

### Host and client

The `host` is either the machine that is sharing out its screen, or the process
or software that allows for such functionality, depending on the context. Use
terms like `host machine` and `host process` to disambiguate if necessary.

The `client` is the website used to connect to (and remotely control) the host,
or the machine that does that, depending on the context. Use terms like `client
machine` and `client website` to disambiguate if necessary. Note that the code
for the client website **is not in this repo**.

### WebRTC and Chromotocol

CRD currently uses WebRTC for communication between the host and the client.
There is obsolete code in our code base for a communication protocol called
Chromotocol, which was used by deprecated non-website clients, which will soon
be deleted.

### Multi-process architecture

Currently only the CRD Windows host has a real multi-process architecture, where
we run a daemon process as SYSTEM at MIC SYSTEM level
(//remoting/host/daemon_process.h), which also acts as a mojo IPC broker. A
network process (//remoting/host/remoting_me2me_host.cc) is run as LOCAL_SERVICE
at MIC LOW level for actually communicating with the client via network
requests. A third desktop process (//remoting/host/desktop_process.h) is run as
SYSTEM at MIC SYSTEM level, which is responsible to capturing screen frame data
and other operations that need to be done on the user session. There are also
user processes such as the remote-open-url process and file transfer process,
which is run by the logged in user with MIC MEDIUM level within the CRD session,
which talks to other CRD processes using Mojo IPC. All CRD processes use Mojo
IPC to communicate with each other. Mojo connection is established by connecting
to the broker process, i.e. the daemon process.

CRD Mac and Linux hosts don't really have a real multi-process architecture. The
Mac host has an agent process broker process
(//remoting/host/mac/agent_process_broker_main.cc), which is used to make sure
only one host process (i.e. network process, but with screen capturing and all
other logic in it) is run at a time. The agent process broker process also acts
as the mojo broker for user processes.

Linux only has one single host process (i.e. network process), and it acts as
the mojo broker for user processes.

## Sanity Checks

Always do the following after making edits.

### Build Targets

Always build relevant targets after making edits. Typical targets could be:

*   `remoting_all` - Build everything that's related to remoting, including
    everything below.
*   `remoting_unittests` - unittests.
*   `remoting_dev_me2me_host` - build a bunch of targets including everything
    below that allows for running the CRD host in the current directory.
*   `remoting_me2me_host` - the Me2Me host binary.
*   `remoting_native_messaging_host` - Native Messaging host for Me2Me.
*   `remote_assistance_host` - Native Messaging host for It2Me.

### Dependencies

Always run `gn check` after making edits to validate target dependencies.
Sometimes you may need to create new build targets to break circular
dependencies. For example, targets like //remoting/host:common are huge and
likely to cause problems. Prefer adding `deps` as opposed to `public_deps` when
possible.

### Formatting

Always run `git cl format` after making edits.
