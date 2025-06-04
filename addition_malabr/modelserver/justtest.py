#!/usr/bin/env python
import time
import tensorflow as tf

def train_model():
    # Load MNIST data.
    (x_train, y_train), _ = tf.keras.datasets.mnist.load_data()
    # Normalize images to [0, 1].
    x_train = x_train.astype('float32') / 255.0

    # Convert labels to one-hot encoding.
    y_train_onehot = tf.keras.utils.to_categorical(y_train, 10)

    # Mimic the tfjs training setup by taking a subset.
    TRAIN_DATA_SIZE = 5500
    x_train = x_train[:TRAIN_DATA_SIZE]
    y_train_onehot = y_train_onehot[:TRAIN_DATA_SIZE]

    # Define a simple model.
    model = tf.keras.Sequential([
        tf.keras.layers.Flatten(input_shape=(28, 28)),
        tf.keras.layers.Dense(128, activation='relu'),
        tf.keras.layers.Dense(10, activation='softmax')
    ])
    
    model.compile(
        optimizer='adam',
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )

    # Start timing the training.
    start_time = time.perf_counter()
    history = model.fit(x_train, y_train_onehot, epochs=10, batch_size=512, verbose=0)
    end_time = time.perf_counter()

    training_time_ms = (end_time - start_time) * 1000  # Convert seconds to milliseconds.
    final_accuracy = history.history['accuracy'][-1]

    print("Training complete.")
    print(f"Training time: {training_time_ms:.2f} ms")
    print(f"Final training accuracy: {final_accuracy*100:.2f}%")

if __name__ == '__main__':
    train_model()
