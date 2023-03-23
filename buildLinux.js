// Script used by buildkite to build Chromium for Linux in CI

const path = require("path");
const { spawnSync } = require("child_process");

const chromium = process.cwd();

try {
  spawnChecked("git", ["pull"], { cwd: chromium, stdio: "inherit" });
} catch (e) {
  // Ignore errors due to being at a detached head.
}

spawnChecked("git", ["fetch"], { cwd: `${chromium}/v8`, stdio: "inherit" });
spawnChecked("git", ["fetch"], {
  cwd: `${chromium}/third_party/skia`,
  stdio: "inherit",
});
spawnChecked("git", ["fetch"], {
  cwd: `${chromium}/third_party/webrtc`,
  stdio: "inherit",
});

const branch = process.env["BUILDKITE_BRANCH"];

spawnChecked("git", ["checkout", branch], {
  cwd: chromium,
  stdio: "inherit",
});

// TODO(dmiller): do we actually need to do this?
// We need to pull again to actually update the checkout ...or do we?
spawnChecked("git", ["pull"], { cwd: chromium, stdio: "inherit" });

spawnChecked("gclient", ["sync"], { cwd: chromium, stdio: "inherit" });

const dockerArgs = [
  "run",
  "-e",
  "GOMA_SERVER_HOST=simpsonite.goma.engflow.com",
  "-e",
  "GOMACTL_USE_PROXY=false",
  "-e",
  "DRIVER_REVISION",
  "-v",
  `${path.join(process.env.HOME, "chromium")}:/chromium`,
  "-v",
  `${path.join(process.env.HOME, "depot_tools")}:/depot_tools`,
  "-v",
  `${path.join(
    process.env.HOME,
    ".goma_client_oauth2_config"
  )}:/home/ubuntu/.goma_client_oauth2_config`,
  "-p",
  "9098:9099",
  "chromium-build-new",
];

spawnChecked("docker", dockerArgs, { stdio: "inherit" });

function spawnChecked(cmd, args, options) {
  const prettyCmd = [cmd].concat(args).join(" ");
  console.error(prettyCmd);

  const rv = spawnSync(cmd, args, options);

  if (rv.status != 0 || rv.error) {
    console.error("Process failed:", rv.error || "");
    console.log(rv.stdout.toString() || "");
    console.error(rv.stderr.toString() || "");
    throw new Error(`Spawned process failed with exit code ${rv.status}`);
  }

  return rv;
}
