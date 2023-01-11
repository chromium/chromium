// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");

const { REPLAY_LOCAL_DRIVER_DIR } = process.env;

if (REPLAY_LOCAL_DRIVER_DIR && process.env.DRIVER_REVISION) {
  // local driver should generally be latest
  throw new Error("Conflicting build settings: environment variables DRIVER_REVISION and REPLAY_LOCAL_DRIVER_DIR cannot coexist.");
}

// Ensure that the git repository is "trusted", otherwise we'll get errors like:
// fatal: unsafe repository ('/chromium/src' is owned by someone else)
spawnChecked("git", [
  "config",
  "--global",
  "--add",
  "safe.directory",
  __dirname,
]);

if (currentPlatform() == "macOS") {
  // Make sure the main executable gets rebuilt with the new build ID.
  spawnChecked("touch", [`${__dirname}/chrome/app/chrome_exe_main_mac.cc`]);
}

if (!REPLAY_LOCAL_DRIVER_DIR) {
  // Download the record/replay driver archive, using the latest version unless
  // it was overridden via the environment.
  console.log(`Downloading driver...`);
  let driverArchive = `${currentPlatform()}-recordreplay.tgz`;
  let downloadArchive = driverArchive;
  if (process.env.DRIVER_REVISION) {
    downloadArchive = `${currentPlatform()}-recordreplay-${
      process.env.DRIVER_REVISION
    }.tgz`;
  }
  spawnChecked(
    "curl",
    [
      `https://static.replay.io/downloads/${downloadArchive}`,
      "-o",
      driverArchive,
    ],
    { stdio: "inherit" }
  );
  spawnChecked("tar", ["xf", driverArchive]);
  fs.unlinkSync(driverArchive);
}


let driverFile = `${currentPlatform()}-recordreplay.${driverExtension()}`;
let driverJSON = `${currentPlatform()}-recordreplay.json`;
if (REPLAY_LOCAL_DRIVER_DIR) {
  driverFile = path.resolve(REPLAY_LOCAL_DRIVER_DIR, driverFile);
  driverJSON = path.resolve(REPLAY_LOCAL_DRIVER_DIR, driverJSON);
}

// Embed the driver in the source.
console.log(`Embedding ${REPLAY_LOCAL_DRIVER_DIR ? 'LOCAL' : 'DOWNLOADED'} driver...`);
const driverContents = fs.readFileSync(driverFile);
const { revision: driverRevision, date: driverDate } = JSON.parse(
  fs.readFileSync(driverJSON, "utf8")
);

if (!REPLAY_LOCAL_DRIVER_DIR) {
  // cleanup
  fs.unlinkSync(driverFile);
  fs.unlinkSync(driverJSON);
}

let driverString = "";
for (let i = 0; i < driverContents.length; i++) {
  driverString += `\\${driverContents[i].toString(8)}`;
}
fs.writeFileSync(
  `${__dirname}/base/record_replay_driver.cc`,
  `
namespace recordreplay {
  char gRecordReplayDriver[] = "${driverString}";
  int gRecordReplayDriverSize = ${driverContents.length};
  char gBuildId[] = "${computeBuildId()}";
}
`
);

console.log(`Preparing...`);
const useGoma = !process.env.NO_GOMA;
if (useGoma) {
  // ensure goma is started for cloud builds with engflow
  spawnChecked("goma_ctl", ["restart"]);
}

// ensure that build configuration is written with correct paths
spawnChecked("gn", ["gen", "out/Release"]);

console.log(`Building...`);
spawnChecked("autoninja", ["-C", "out/Release", "chrome"], {
  stdio: "inherit",
});

console.log(`Build finished.`);


function spawnChecked(cmd, args, options) {
  const prettyCmd = [cmd].concat(args).join(" ");
  console.error(prettyCmd);

  const rv = spawnSync(cmd, args, options);

  if (rv.status != 0 || rv.error) {
    console.error('Process failed:', rv.error || '');
    console.log(rv.stdout.toString() || '');
    console.error(rv.stderr.toString() || '');
    throw new Error(`Spawned process failed with exit code ${rv.status}`);
  }

  return rv;
}

function currentPlatform() {
  switch (process.platform) {
    case "darwin":
      return "macOS";
    case "linux":
      return "linux";
    default:
      throw new Error(`Platform ${process.platform} not supported`);
  }
}

function driverExtension() {
  return currentPlatform() == "windows" ? "dll" : "so";
}

function computeBuildId() {
  // Note: this build ID doesn't include revision etc. information for v8 or other inner git
  // repositories. It would be good to either fix this or enforce that the chromium revision
  // gets bumped whenever an inner repository changes.
  const chromiumRevision = spawnChecked("git", [
    "rev-parse",
    "--short=12",
    "HEAD",
  ])
    .stdout.toString()
    .trim();
  const chromiumDate = spawnChecked("git", [
    "show",
    "HEAD",
    "--pretty=%cd",
    "--date=short",
    "--no-patch",
  ])
    .stdout.toString()
    .trim()
    .replace(/-/g, "");

  // Use the later of the two dates in the build ID.
  const date = +chromiumDate >= +driverDate ? chromiumDate : driverDate;

  return `${currentPlatform()}-chromium-${date}-${chromiumRevision}-${driverRevision}`;
}
