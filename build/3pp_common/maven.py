# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The fetch.py and install.py helpers for a 3pp Maven module."""

import os
import pathlib
import re
import shutil
import subprocess
import urllib.request

import common

APACHE_MAVEN_URL = 'https://repo.maven.apache.org/maven2'

_POM_TEMPLATE = """\
<project>
  <modelVersion>4.0.0</modelVersion>
  <groupId>group</groupId>
  <artifactId>artifact</artifactId>
  <version>1</version>
  <dependencies>
    <dependency>
      <groupId>{group_id}</groupId>
      <artifactId>{artifact_id}</artifactId>
      <version>{version}</version>
      <scope>runtime</scope>
    </dependency>
  </dependencies>
  <build>
    <plugins>
      <plugin>
        <artifactId>maven-assembly-plugin</artifactId>
        <version>3.3.0</version>
        <configuration>
          <descriptorRefs>
            <descriptorRef>jar-with-dependencies</descriptorRef>
          </descriptorRefs>
        </configuration>
        <executions>
          <execution>
            <phase>package</phase>
            <goals>
              <goal>single</goal>
            </goals>
          </execution>
        </executions>
      </plugin>
    </plugins>
  </build>
  <repositories>
    <repository>
      <id>placeholder_id</id>
      <name>placeholder_name</name>
      <url>{maven_url}</url>
    </repository>
  </repositories>
</project>
"""


def _detect_latest(maven_url, package):
    metadata_url = '{}/{}/maven-metadata.xml'.format(
        maven_url,
        package.replace('.', '/').replace(':', '/'))
    metadata = urllib.request.urlopen(metadata_url).read().decode('utf-8')
    # Do not parse xml with the Python included parser since it is susceptible
    # to maliciously crafted xmls. Only use regular expression parsing to be
    # safe. RE should be enough to handle what we need to extract.
    # TODO(agrieve): Use 'if m := ..." once docker image updates from Python 3.6.
    m = re.search('<latest>([^<]+)</latest>', metadata)
    if m:
        latest = m.group(1)
    else:
        # If no latest info was found just hope the versions are sorted and the
        # last one is the latest (as is commonly the case).
        latest = re.findall('<version>([^<]+)</version>', metadata)[-1]
    return latest


def _install(output_prefix,
             deps_prefix,
             maven_url,
             package,
             version,
             jar_name=None):
    # Runs in a docker container.
    group_id, artifact_id = package.split(':')
    if not jar_name:
        jar_name = f'{artifact_id}.jar'

    pathlib.Path('pom.xml').write_text(
        _POM_TEMPLATE.format(version=version,
                             group_id=group_id,
                             artifact_id=artifact_id,
                             maven_url=maven_url))

    # Set up JAVA_HOME for the mvn command to find the JDK.
    env = os.environ.copy()
    env['JAVA_HOME'] = common.path_within_checkout('third_party/jdk/current')

    # Ensure that mvn works and the environment is set up correctly.
    subprocess.run(['mvn', '-v'], check=True, env=env)

    # Build the jar file, explicitly specify -f to reduce sources of error.
    subprocess.run(['mvn', 'clean', 'assembly:single', '-f', 'pom.xml'],
                   check=True,
                   env=env)

    # Move and rename output to the upload directory. Moving only the jar avoids
    # polluting the output directory with maven intermediate outputs.
    shutil.move('target/artifact-1-jar-with-dependencies.jar',
                os.path.join(output_prefix, jar_name))


def main(*,
         package,
         jar_name=None,
         maven_url='https://dl.google.com/android/maven2',
         version_override=None):
    """3pp entry point for fetch.py.

    Args:
      package: E.g.: some.package:some-thing
      jar_name: Name of .jar. Defaults to |some-thing|.jar
      maven_url: URL of Maven repository.
      version_override: Use this version instead of the latest one.
    """

    def do_latest():
        return version_override or _detect_latest(maven_url, package)

    def do_install(args):
        _install(args.output_prefix, args.deps_prefix, maven_url, package,
                 args.version, jar_name)

    common.main(do_latest=do_latest,
                do_install=do_install,
                runtime_deps=['//third_party/jdk/current'])
